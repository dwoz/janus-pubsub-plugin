#include <jansson.h>
#include <sys/types.h>
#include <plugins/plugin.h>
#include <debug.h>
#include <config.h>
#include <mutex.h>
#include <rtp.h>
#include <rtcp.h>
#include <record.h>
#include <sdp-utils.h>
#include <utils.h>
#include <curl/curl.h>

#define JANUS_PUBSUB_VERSION 1
#define JANUS_PUBSUB_VERSION_STRING "0.0.1"
#define JANUS_PUBSUB_DESCRIPTION "The Janus PubSub plugin."
#define JANUS_PUBSUB_NAME "Janus PubSub plugin"
#define JANUS_PUBSUB_AUTHOR "Daniel Wozniak"
#define JANUS_PUBSUB_PACKAGE "janus.plugin.pubsub"


janus_plugin *create(void);
int janus_pubsub_init(janus_callbacks *callback, const char *config_path);
void janus_pubsub_destroy(void);
int janus_pubsub_get_api_compatibility(void);
int janus_pubsub_get_version(void);
const char *janus_pubsub_get_version_string(void);
const char *janus_pubsub_get_description(void);
const char *janus_pubsub_get_name(void);
const char *janus_pubsub_get_author(void);
const char *janus_pubsub_get_package(void);
void janus_pubsub_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_pubsub_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_pubsub_setup_media(janus_plugin_session *handle);
void janus_pubsub_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_pubsub_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_pubsub_incoming_data(janus_plugin_session *handle, char *buf, int len);
void janus_pubsub_slow_link(janus_plugin_session *handle, int uplink, int video);
void janus_pubsub_hangup_media(janus_plugin_session *handle);
void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error);
static void *janus_pubsub_handler(void *data);
json_t *janus_pubsub_query_session(janus_plugin_session *handle);


static janus_plugin janus_pubsub_plugin =
    JANUS_PLUGIN_INIT (
        .init = janus_pubsub_init,
        .destroy = janus_pubsub_destroy,

        .get_api_compatibility = janus_pubsub_get_api_compatibility,
        .get_version = janus_pubsub_get_version,
        .get_version_string = janus_pubsub_get_version_string,
        .get_description = janus_pubsub_get_description,
        .get_name = janus_pubsub_get_name,
        .get_author = janus_pubsub_get_author,
        .get_package = janus_pubsub_get_package,

        .create_session = janus_pubsub_create_session,
        .handle_message = janus_pubsub_handle_message,
        .setup_media = janus_pubsub_setup_media,
        .incoming_rtp = janus_pubsub_incoming_rtp,
        .incoming_rtcp = janus_pubsub_incoming_rtcp,
        .incoming_data = janus_pubsub_incoming_data,
        .slow_link = janus_pubsub_slow_link,
        .hangup_media = janus_pubsub_hangup_media,
        .destroy_session = janus_pubsub_destroy_session,
        .query_session = janus_pubsub_query_session,
    );

static volatile gint initialized = 0, stopping = 0;
static gboolean notify_events = TRUE;
static GThread *handler_thread;
static GThread *watchdog;

typedef struct janus_pubsub_message {
    janus_plugin_session *handle;
    char *transaction;
    json_t *message;
    json_t *jsep;
} janus_pubsub_message;

static GAsyncQueue *messages = NULL;
static janus_pubsub_message exit_message;

typedef struct janus_pubsub_session {
    janus_plugin_session *handle;
        gchar *stream_name;
    gboolean has_audio;
    gboolean has_video;
    gboolean has_data;
    gboolean audio_active;
    gboolean video_active;
    uint32_t bitrate;
    janus_recorder *arc;    /* The Janus recorder instance for this user's audio, if enabled */
    janus_recorder *vrc;    /* The Janus recorder instance for this user's video, if enabled */
    janus_recorder *drc;    /* The Janus recorder instance for this user's data, if enabled */
    janus_mutex rec_mutex;    /* Mutex to protect the recorders from race conditions */
    guint16 slowlink_count;
    volatile gint hangingup;
    gint64 destroyed;    /* Time at which this session was marked as destroyed */
} janus_pubsub_session;

typedef struct jansus_pubsub_stream {
        gchar *name;
    gchar *sdp;            /* The SDP this publisher negotiated, if any */
    gchar *sdp_type;
        janus_pubsub_session *publisher;
    GHashTable *subscribers;

} janus_pubsub_stream;

static GHashTable *streams;
static GHashTable *sessions;
static GList *old_sessions;
static janus_mutex sessions_mutex;
static janus_callbacks *gateway = NULL;

/* Error codes */
#define JANUS_PUBSUB_ERROR_NO_MESSAGE            411
#define JANUS_PUBSUB_ERROR_INVALID_JSON        412
#define JANUS_PUBSUB_ERROR_INVALID_ELEMENT    413
#define JANUS_PUBSUB_ERROR_INVALID_SDP        414

static void janus_pubsub_message_free(janus_pubsub_message *msg) {
    if(!msg || msg == &exit_message)
        return;

    msg->handle = NULL;

    g_free(msg->transaction);
    msg->transaction = NULL;
    if(msg->message)
        json_decref(msg->message);
    msg->message = NULL;
    if(msg->jsep)
        json_decref(msg->jsep);
    msg->jsep = NULL;

    g_free(msg);
}

janus_plugin *create(void) {
    JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_PUBSUB_NAME);
    return &janus_pubsub_plugin;
}

/* PubSub watchdog/garbage collector (sort of) */
static void *janus_pubsub_watchdog(void *data) {
    JANUS_LOG(LOG_INFO, "PubSub watchdog started\n");
    gint64 now = 0;
    while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
        janus_mutex_lock(&sessions_mutex);
        /* Iterate on all the sessions */
        now = janus_get_monotonic_time();
        if(old_sessions != NULL) {
            GList *sl = old_sessions;
            JANUS_LOG(LOG_HUGE, "Checking %d old PubSub sessions...\n", g_list_length(old_sessions));
            while(sl) {
                janus_pubsub_session *session = (janus_pubsub_session *)sl->data;
                if(!session) {
                    sl = sl->next;
                    continue;
                }
                if(now-session->destroyed >= 5*G_USEC_PER_SEC) {
                    /* We're lazy and actually get rid of the stuff only after a few seconds */
                    JANUS_LOG(LOG_VERB, "Freeing old PubSub session\n");
                    GList *rm = sl->next;
                    old_sessions = g_list_delete_link(old_sessions, sl);
                    sl = rm;
                    session->handle = NULL;
                    g_free(session);
                    session = NULL;
                    continue;
                }
                sl = sl->next;
            }
        }
        /* TODO Teardown streams too */
        janus_mutex_unlock(&sessions_mutex);
        g_usleep(500000);
    }
    JANUS_LOG(LOG_INFO, "PubSub watchdog stopped\n");
    return NULL;
}

int janus_pubsub_init(janus_callbacks *callback, const char *config_path) {
    if(callback == NULL || config_path == NULL) {
        /* Invalid arguments */
        return -1;
    }

    /* Read configuration */
    char filename[255];
    g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_PUBSUB_PACKAGE);
    JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
    janus_config *config = janus_config_parse(filename);
    if(config != NULL) {
        janus_config_print(config);
        janus_config_item *events = janus_config_get_item_drilldown(config, "general", "events");
        if(events != NULL && events->value != NULL)
            notify_events = janus_is_true(events->value);
        if(!notify_events && callback->events_is_enabled()) {
            JANUS_LOG(LOG_WARN, "Notification of events to handlers disabled for %s\n", JANUS_PUBSUB_NAME);
        }
    }
    janus_config_destroy(config);
    config = NULL;
    gateway = callback;
    streams = g_hash_table_new(g_str_hash, g_str_equal);
    sessions = g_hash_table_new(NULL, NULL);
    janus_mutex_init(&sessions_mutex);
    g_atomic_int_set(&initialized, 1);
    messages = g_async_queue_new_full((GDestroyNotify) janus_pubsub_message_free);
    GError *error = NULL;
    /* Start the sessions watchdog */
    watchdog = g_thread_try_new("pubsub watchdog", &janus_pubsub_watchdog, NULL, &error);
    if(error != NULL) {
        g_atomic_int_set(&initialized, 0);
        JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the PubSub watchdog thread...\n",
                        error->code, error->message ? error->message : "??");
        return -1;
    }
        /* Start message handler thread */
    handler_thread = g_thread_try_new("pubsub handler", janus_pubsub_handler, NULL, &error);
    if(error != NULL) {
        g_atomic_int_set(&initialized, 0);
        JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the PubSub handler thread...\n",
                        error->code, error->message ? error->message : "??");
        return -1;
    }
        curl_global_init(CURL_GLOBAL_ALL);
    JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_PUBSUB_NAME);
    return 0;
}


void janus_pubsub_destroy(void) {
    if(!g_atomic_int_get(&initialized))
        return;
    g_atomic_int_set(&stopping, 1);
    g_async_queue_push(messages, &exit_message);
    if(handler_thread != NULL) {
        g_thread_join(handler_thread);
        handler_thread = NULL;
    }
    if(watchdog != NULL) {
        g_thread_join(watchdog);
        watchdog = NULL;
    }
    /* FIXME Destroy the sessions cleanly */
    janus_mutex_lock(&sessions_mutex);
    // TODO
        // g_hash_table_destroy(streams);
    g_hash_table_destroy(sessions);
    janus_mutex_unlock(&sessions_mutex);
    sessions = NULL;
    g_atomic_int_set(&initialized, 0);
    g_atomic_int_set(&stopping, 0);
    JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_PUBSUB_NAME);
}

int janus_pubsub_get_api_compatibility(void) {
    return JANUS_PLUGIN_API_VERSION;
}

int janus_pubsub_get_version(void) {
    return JANUS_PUBSUB_VERSION;
}

const char *janus_pubsub_get_version_string(void) {
    return JANUS_PUBSUB_VERSION_STRING;
}

const char *janus_pubsub_get_description(void) {
    return JANUS_PUBSUB_DESCRIPTION;
}

const char *janus_pubsub_get_name(void) {
    return JANUS_PUBSUB_NAME;
}

const char *janus_pubsub_get_author(void) {
    return JANUS_PUBSUB_AUTHOR;
}

const char *janus_pubsub_get_package(void) {
    return JANUS_PUBSUB_PACKAGE;
}

static janus_pubsub_session *janus_pubsub_lookup_session(janus_plugin_session *handle) {
    janus_pubsub_session *session = NULL;
    if (g_hash_table_contains(sessions, handle)) {
        session = (janus_pubsub_session *)handle->plugin_handle;
    }
    return session;
}

void janus_pubsub_create_session(janus_plugin_session *handle, int *error) {
    if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
        *error = -1;
        return;
    }
    janus_pubsub_session *session = (janus_pubsub_session *)g_malloc0(sizeof(janus_pubsub_session));
    session->handle = handle;
    session->has_audio = FALSE;
    session->has_video = FALSE;
    session->has_data = FALSE;
    session->audio_active = TRUE;
    session->video_active = TRUE;
        session->stream_name = NULL;
    janus_mutex_init(&session->rec_mutex);
    session->bitrate = 0;    /* No limit */
    session->destroyed = 0;
    g_atomic_int_set(&session->hangingup, 0);
    handle->plugin_handle = session;
    janus_mutex_lock(&sessions_mutex);
    g_hash_table_insert(sessions, handle, session);
    janus_mutex_unlock(&sessions_mutex);
    JANUS_LOG(LOG_INFO, "PubSub Session created.\n");
}

void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error) {
    if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
        *error = -1;
        return;
    }
    janus_pubsub_session *session = (janus_pubsub_session *)handle->plugin_handle;
    if(!session) {
        JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
        *error = -2;
        return;
    }
    JANUS_LOG(LOG_VERB, "Removing PubSub session...\n");
    janus_mutex_lock(&sessions_mutex);
    if(!session->destroyed) {
        session->destroyed = janus_get_monotonic_time();
        g_hash_table_remove(sessions, handle);
        /* Cleaning up and removing the session is done in a lazy way */
        old_sessions = g_list_append(old_sessions, session);
    }
    janus_mutex_unlock(&sessions_mutex);
        curl_global_cleanup();
    JANUS_LOG(LOG_INFO, "PubSub Session destroyed.\n");
}


json_t *janus_pubsub_query_session(janus_plugin_session *handle) {
    if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
        return NULL;
    }
    janus_mutex_lock(&sessions_mutex);
    janus_pubsub_session *session = janus_pubsub_lookup_session(handle);
    if(!session) {
        janus_mutex_unlock(&sessions_mutex);
        JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
        return NULL;
    }
    /* In the echo test, every session is the same: we just provide some configure info */
    json_t *info = json_object();
    janus_mutex_unlock(&sessions_mutex);
    // XXX: Populate info
    return info;
}


struct janus_plugin_result *janus_pubsub_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {

    if(g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
        return janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized", NULL);

    janus_pubsub_message *msg = g_malloc0(sizeof(janus_pubsub_message));
    msg->handle = handle;
    msg->transaction = transaction;
    msg->message = message;
    msg->jsep = jsep;
    g_async_queue_push(messages, msg);

    /* All the requests to this plugin are handled asynchronously: we add a comment
     * (a JSON object with a "hint" string in it, that's what the core expects),
     * but we don't have to: other plugins don't put anything in there */
    return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, "I'm taking my time!", NULL);

    JANUS_LOG(LOG_INFO, "PubSub got message. (%s)\n", json_object_get(message, "video"));
    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, NULL);
}

void janus_pubsub_setup_media(janus_plugin_session *handle) {
    JANUS_LOG(LOG_INFO, "WebRTC media is now available.\n");
}


void janus_pubsub_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len) {
    if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
        return;
    /* Simple echo test */
    if(gateway) {
        /* Honour the audio/video active flags */
        janus_pubsub_session *session = (janus_pubsub_session *)handle->plugin_handle;
        if(!session) {
            JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
            return;
        }
        if(session->destroyed)
            return;

        janus_pubsub_stream *stream = g_hash_table_lookup(streams, session->stream_name);
        int count = 0;
        GHashTableIter iter;
        gpointer value;
        g_hash_table_iter_init(&iter, stream->subscribers);
        while (!session->destroyed && g_hash_table_iter_next(&iter, NULL, &value)) {
            if (once_c == 0) {
                JANUS_LOG(LOG_WARN, "RTP Relay %d bytes\n", len);
                once_c = 1;
            }
            janus_pubsub_session *p = value;
            gateway->relay_rtp(p->handle, video, buf, len);
        }
        //if((!video && session->audio_active) || (video && session->video_active)) {
        //    /* Save the frame if we're recording */
        //    janus_recorder_save_frame(video ? session->vrc : session->arc, buf, len);
        //    /* Send the frame back */
        //    gateway->relay_rtp(handle, video, buf, len);
        //}
    }
    JANUS_LOG(LOG_VERB, "OUT - Got an RTP message (%d bytes.)\n", len);
}


void janus_pubsub_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len) {
    if(handle == NULL || handle->stopped || g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized))
        return;
    /* Simple echo test */
    if(gateway) {
        janus_pubsub_session *session = (janus_pubsub_session *)handle->plugin_handle;
        if(!session) {
            JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
            return;
        }
        if(session->destroyed)
            return;
                if (session->stream_name == NULL) {
            JANUS_LOG(LOG_ERR, "RTCP with no stream...\n");
            return;
                }
        janus_pubsub_stream *stream = g_hash_table_lookup(streams, session->stream_name);
        guint32 bitrate = janus_rtcp_get_remb(buf, len);
        if (session->handle == stream->publisher->handle) {
            int count = 0;
            GHashTableIter iter;
            gpointer value;
            g_hash_table_iter_init(&iter, stream->subscribers);
            while (!session->destroyed && g_hash_table_iter_next(&iter, NULL, &value)) {
                janus_pubsub_session *p = value;
                if(bitrate > 0) {
                    /* If a REMB arrived, make sure we cap it to our configuration, and send it as a video RTCP */
                    if(session->bitrate > 0)
                        janus_rtcp_cap_remb(buf, len, session->bitrate);
                    gateway->relay_rtcp(p->handle, 1, buf, len);
                    continue;
                }
                gateway->relay_rtcp(p->handle, video, buf, len);
            }
        } else {
            if(bitrate > 0) {
                /* If a REMB arrived, make sure we cap it to our configuration, and send it as a video RTCP */
                if(session->bitrate > 0)
                    janus_rtcp_cap_remb(buf, len, session->bitrate);
                gateway->relay_rtcp(stream->publisher->handle, 1, buf, len);
                return;
            }
            gateway->relay_rtcp(stream->publisher->handle, video, buf, len);
        }
    }
    JANUS_LOG(LOG_VERB, "OUT - Got an RTCP message (%d bytes.)\n", len);
}

void janus_pubsub_incoming_data(janus_plugin_session *handle, char *buf, int len) {
    JANUS_LOG(LOG_VERB, "Got a DataChannel message (%d bytes.)\n", len);
}

void janus_pubsub_slow_link(janus_plugin_session *handle, int uplink, int video) {
    JANUS_LOG(LOG_VERB, "Slow link detected.\n");
}

void janus_pubsub_hangup_media(janus_plugin_session *handle) {
    JANUS_LOG(LOG_INFO, "No WebRTC media anymore.\n");
}

/* Thread to handle incoming messages */
static void *janus_pubsub_handler(void *data) {
    JANUS_LOG(LOG_VERB, "Joining PubSub handler thread\n");
    janus_pubsub_message *msg = NULL;
    int error_code = 0;
    char *error_cause = g_malloc0(512);
    json_t *root = NULL;
    while(g_atomic_int_get(&initialized) && !g_atomic_int_get(&stopping)) {
        msg = g_async_queue_pop(messages);
        if(msg == NULL)
            continue;
        if(msg == &exit_message)
            break;
        if(msg->handle == NULL) {
            janus_pubsub_message_free(msg);
            continue;
        }
        janus_pubsub_session *session = NULL;
        janus_mutex_lock(&sessions_mutex);
        if(g_hash_table_lookup(sessions, msg->handle) != NULL ) {
            session = (janus_pubsub_session *)msg->handle->plugin_handle;
        }
        if(!session) {
            janus_mutex_unlock(&sessions_mutex);
            JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
            janus_pubsub_message_free(msg);
            continue;
        }
        if(session->destroyed) {
            janus_mutex_unlock(&sessions_mutex);
            janus_pubsub_message_free(msg);
            continue;
        }
        janus_mutex_unlock(&sessions_mutex);

        /* Handle request */
        error_code = 0;
        root = msg->message;
        if(msg->message == NULL) {
            JANUS_LOG(LOG_ERR, "No message??\n");
            error_code = JANUS_PUBSUB_ERROR_NO_MESSAGE;
            g_snprintf(error_cause, 512, "%s", "No message??");
            goto error;
        }
        if(!json_is_object(root)) {
            JANUS_LOG(LOG_ERR, "JSON error: not an object\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_JSON;
            g_snprintf(error_cause, 512, "JSON error: not an object");
            goto error;
        }

        json_t *audio = json_object_get(root, "audio");
        if(audio && !json_is_boolean(audio)) {
            JANUS_LOG(LOG_ERR, "Invalid element (audio should be a boolean)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (audio should be a boolean)");
            goto error;
        }
        json_t *video = json_object_get(root, "video");
        if(video && !json_is_boolean(video)) {
            JANUS_LOG(LOG_ERR, "Invalid element (video should be a boolean)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (video should be a boolean)");
            goto error;
        }
        json_t *data = json_object_get(root, "data");
        if(data && !json_is_boolean(data)) {
            JANUS_LOG(LOG_ERR, "Invalid element (video should be a boolean)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (video should be a boolean)");
            goto error;
        }
        json_t *bitrate = json_object_get(root, "bitrate");
        if(bitrate && (!json_is_integer(bitrate) || json_integer_value(bitrate) < 0)) {
            JANUS_LOG(LOG_ERR, "Invalid element (bitrate should be a positive integer)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (bitrate should be a positive integer)");
            goto error;
        }
        json_t *record = json_object_get(root, "record");
        if(record && !json_is_boolean(record)) {
            JANUS_LOG(LOG_ERR, "Invalid element (record should be a boolean)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (record should be a boolean)");
            goto error;
        }
        json_t *recfile = json_object_get(root, "filename");
        if(recfile && !json_is_string(recfile)) {
            JANUS_LOG(LOG_ERR, "Invalid element (filename should be a string)\n");
            error_code = JANUS_PUBSUB_ERROR_INVALID_ELEMENT;
            g_snprintf(error_cause, 512, "Invalid value (filename should be a string)");
            goto error;
        }

        /* Parse request */
        const char *msg_sdp_type = json_string_value(json_object_get(msg->jsep, "type"));
        const char *msg_sdp = json_string_value(json_object_get(msg->jsep, "sdp"));

        json_t *request = json_object_get(root, "request");
        const char *request_text = json_string_value(request);
        janus_pubsub_stream *stream = NULL;
        if(!strcasecmp(request_text, "publish")) {
            json_t *name = json_object_get(root, "name");
            char *fook = malloc(2048);
            const char *publish_name = json_string_value(name);

            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");

            /* Example curl request */
            CURL *curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/publish");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            json_t *post_msg = json_pack("{soso}", "msg", root, "jesp", msg->jsep);
            char *post_data = json_dumps(post_msg, JSON_ENCODE_ANY);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
            CURLcode res = curl_easy_perform(curl);

            if(res != CURLE_OK) {
                JANUS_LOG(LOG_WARN, "CURL RESP NOT OK \n");
            } else {
                stream = g_malloc0(sizeof(janus_pubsub_stream));
                stream->name = g_strdup(publish_name);
                stream->publisher = session;
                stream->subscribers = g_hash_table_new(NULL, NULL);
                stream->sdp = NULL;
                stream->sdp_type = NULL;
                session->stream_name = g_strdup(stream->name);
                g_hash_table_insert(streams, g_strdup(publish_name), stream);
                JANUS_LOG(LOG_WARN, "CURL RESP OK (%s)\n", stream->name);
            }
            curl_easy_cleanup(curl);
            free(post_data);
        }
        if(!strcasecmp(request_text, "play")) {
            json_t *name = json_object_get(root, "name");
            const char *play_name = json_string_value(name);

            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");

            /* Example curl request */
            CURL *curl = curl_easy_init();
            curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:5000/play");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            json_t *post_msg = json_pack("{so}", "msg", root);
            char *post_data = json_dumps(post_msg, JSON_ENCODE_ANY);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);


            CURLcode res = curl_easy_perform(curl);
            if(res != CURLE_OK) {
                JANUS_LOG(LOG_WARN, "CURL PLAY RESP NOT OK \n");
            } else {
                json_t *event_x = json_object();
                json_object_set_new(event_x, "pubsub", json_string("event"));
                json_object_set_new(event_x, "result", json_string("ok"));
                stream = g_hash_table_lookup(streams, play_name);
                if (stream != NULL) {
                    g_hash_table_insert(stream->subscribers, session->handle, session);
                    session->stream_name = g_strdup(stream->name);
                    json_t *jsep_x = json_pack("{ssss}", "type", stream->sdp_type, "sdp", stream->sdp);
                    int res = gateway->push_event(msg->handle, &janus_pubsub_plugin, msg->transaction, event_x, jsep_x);
                    json_decref(event_x);
                    json_decref(jsep_x);
                    JANUS_LOG(LOG_WARN, "CURL PLAY RESP OK (%s) \n", stream->name);
                } else {
                    JANUS_LOG(LOG_WARN, "CURL PLAY RESP OK BUT NO STREAM \n");
                }
            }
            curl_easy_cleanup(curl);
            free(post_data);
        }
        if(!strcasecmp(request_text, "play")) {
            continue;
        }

        /* Enforce request */
        if(audio) {
            session->audio_active = json_is_true(audio);
            JANUS_LOG(LOG_VERB, "Setting audio property: %s\n", session->audio_active ? "true" : "false");
        }
        if(video) {
            if(!session->video_active && json_is_true(video)) {
                /* Send a PLI */
                JANUS_LOG(LOG_VERB, "Just (re-)enabled video, sending a PLI to recover it\n");
                char buf[12];
                memset(buf, 0, 12);
                janus_rtcp_pli((char *)&buf, 12);
                gateway->relay_rtcp(session->handle, 1, buf, 12);
            }
            session->video_active = json_is_true(video);
            JANUS_LOG(LOG_VERB, "Setting video property: %s\n", session->video_active ? "true" : "false");
        }
        if(bitrate) {
            session->bitrate = json_integer_value(bitrate);
            JANUS_LOG(LOG_VERB, "Setting video bitrate: %"SCNu32"\n", session->bitrate);
            if(session->bitrate > 0) {
                /* FIXME Generate a new REMB (especially useful for Firefox, which doesn't send any we can cap later) */
                char buf[24];
                memset(buf, 0, 24);
                janus_rtcp_remb((char *)&buf, 24, session->bitrate);
                JANUS_LOG(LOG_VERB, "Sending REMB\n");
                gateway->relay_rtcp(session->handle, 1, buf, 24);
                /* FIXME How should we handle a subsequent "no limit" bitrate? */
            }
        }

        /* Any SDP to handle? */
        if(msg_sdp) {
            JANUS_LOG(LOG_VERB, "This is involving a negotiation (%s) as well:\n%s\n", msg_sdp_type, msg_sdp);
            session->has_audio = (strstr(msg_sdp, "m=audio") != NULL);
            session->has_video = (strstr(msg_sdp, "m=video") != NULL);
            session->has_data = (strstr(msg_sdp, "DTLS/SCTP") != NULL);

            json_t *event = json_object();
            json_object_set_new(event, "pubsub", json_string("event"));
            json_object_set_new(event, "result", json_string("ok"));
            // Answer the offer and send it to the gateway, to start the echo test //
            const char *type = "answer";
            char error_str[512];
            janus_sdp *answer = NULL;
            janus_sdp *offer = NULL;
            if(!strcasecmp(request_text, "publish")) {
                offer = janus_sdp_parse(msg_sdp, error_str, sizeof(error_str));
                if(offer == NULL) {
                    json_decref(event);
                    JANUS_LOG(LOG_ERR, "Error parsing offer: %s\n", error_str);
                    error_code = JANUS_PUBSUB_ERROR_INVALID_SDP;
                    g_snprintf(error_cause, 512, "Error parsing offer: %s", error_str);
                    goto error;
                }
                answer = janus_sdp_generate_answer(offer, JANUS_SDP_OA_DONE);
                char *answer_sdp = janus_sdp_write(answer);
                offer = janus_sdp_generate_offer(answer->s_name, answer->c_addr,
                    JANUS_SDP_OA_AUDIO, TRUE,
                    //JANUS_SDP_OA_AUDIO_CODEC, janus_videoroom_audiocodec_name(videoroom->acodec),
                    //JANUS_SDP_OA_AUDIO_PT, janus_videoroom_audiocodec_pt(videoroom->acodec),
                    JANUS_SDP_OA_AUDIO_DIRECTION, JANUS_SDP_SENDONLY,
                    JANUS_SDP_OA_VIDEO, TRUE,
                    //JANUS_SDP_OA_VIDEO_CODEC, janus_videoroom_videocodec_name(videoroom->vcodec),
                    //JANUS_SDP_OA_VIDEO_PT, janus_videoroom_videocodec_pt(videoroom->vcodec),
                    JANUS_SDP_OA_VIDEO_DIRECTION, JANUS_SDP_SENDONLY,
                    JANUS_SDP_OA_DATA, TRUE,
                    JANUS_SDP_OA_DONE);
                char *offer_sdp = janus_sdp_write(offer);
                stream->sdp_type = g_strdup(msg_sdp_type);
                stream->sdp = g_strdup(offer_sdp);
                json_t *jsep = json_pack("{ssss}", "type", type, "sdp", answer_sdp);
                //gint64 start = janus_get_monotonic_time();
                int res = gateway->push_event(msg->handle, &janus_pubsub_plugin, msg->transaction, event, jsep);
                //JANUS_LOG(LOG_VERB,
                //          "  >> Pushing event to peer: %d (%s)\n",
                //          ret, janus_get_api_error(ret));
                json_decref(event);
                json_decref(jsep);
                g_free(offer_sdp);
                g_free(answer_sdp);
            }


        }

        //TODO

        janus_pubsub_message_free(msg);
        continue;
error:
        {
            /* Prepare JSON error event */
            json_t *event = json_object();
            json_object_set_new(event, "pubsub", json_string("event"));
            json_object_set_new(event, "error_code", json_integer(error_code));
            json_object_set_new(event, "error", json_string(error_cause));
            int ret = gateway->push_event(msg->handle, &janus_pubsub_plugin, msg->transaction, event, NULL);
            JANUS_LOG(LOG_VERB, "  >> %d (%s)\n", ret, janus_get_api_error(ret));
            janus_pubsub_message_free(msg);
            /* We don't need the event anymore */
            json_decref(event);
        }
    }
    g_free(error_cause);
    JANUS_LOG(LOG_VERB, "Leaving PubSub handler thread\n");
    return NULL;
}
