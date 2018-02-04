#include <glib.h>
#include <plugins/plugin.h>

#include <utils.h>
#include <debug.h>

#include "janus_pubsub.h"
#include "session.h"
#include "stream.h"

static GHashTable *sessions;

void janus_pubsub_sessions_init(void) {
    sessions = g_hash_table_new(NULL, NULL);
}

void janus_pubsub_sessions_destroy(void) {
    g_hash_table_destroy(sessions);
    sessions = NULL;
}

janus_pubsub_session *janus_pubsub_session_get(janus_plugin_session *handle) {
    return g_hash_table_lookup(sessions, handle);
}

gboolean janus_pubsub_has_session(janus_plugin_session *handle) {
    return g_hash_table_contains(sessions, handle);
}

void janus_pubsub_create_session(janus_plugin_session *handle, int *error) {
    JANUS_LOG(LOG_INFO, "Create PubSub Session called.\n");
    if(janus_pubsub_is_stopping() || !janus_pubsub_is_initialized()) {
        *error = -1;
        return;
    }
    JANUS_LOG(LOG_INFO, "Create PubSub Session called 2.\n");
    janus_pubsub_session *session = (janus_pubsub_session *)g_malloc0(sizeof(janus_pubsub_session));
    JANUS_LOG(LOG_INFO, "Create PubSub Session called 2.\n");
    session->handle = handle;
    session->kind = JANUS_SESSION_NONE;
    session->has_audio = FALSE;
    session->has_video = FALSE;
    session->has_data = FALSE;
    session->audio_active = FALSE;
    session->video_active = FALSE;
    session->stream_name = NULL;
    session->sub_id = 0;
    janus_mutex_init(&session->rec_mutex);
    session->bitrate = 0;    /* No limit */
    session->destroyed = 0;
    g_atomic_int_set(&session->hangingup, 0);
    handle->plugin_handle = session;
    janus_mutex_lock(&pubsub_sessions_mutex);
    g_hash_table_insert(sessions, handle, session);
    janus_mutex_unlock(&pubsub_sessions_mutex);
    JANUS_LOG(LOG_INFO, "PubSub Session created.\n");
}

void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error) {
    if(janus_pubsub_is_stopping() || !janus_pubsub_is_initialized()) {
        *error = -1;
        return;
    }
    janus_pubsub_session *session = (janus_pubsub_session *)handle->plugin_handle;
    if(!session) {
        JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
        *error = -2;
        return;
    }
    JANUS_LOG(LOG_INFO, "Removing PubSub session...\n");
    janus_mutex_lock(&pubsub_sessions_mutex);
    JANUS_LOG(LOG_INFO, "Sessions locked\n");
    if(!session->destroyed) {
        janus_mutex_lock(&pubsub_streams_mutex);
        if (session->stream_name != NULL) {
            //janus_pubsub_stream *stream = g_hash_table_lookup(pubsub_streams, session->stream_name);
            janus_pubsub_stream *stream = janus_pubsub_stream_get(session->stream_name);
            if (!stream || stream->destroyed) {
                 JANUS_LOG(LOG_ERR, "Streams hashtable lookup failed...\n");
                 *error = -2;
                 janus_mutex_unlock(&pubsub_streams_mutex);
                 janus_mutex_unlock(&pubsub_sessions_mutex);
                 return;
            }
            if (stream->publisher && session->handle == stream->publisher->handle) {
                stream->destroyed = janus_get_monotonic_time();
                g_hash_table_remove(pubsub_streams, session->stream_name);
                pubsub_old_streams = g_list_append(pubsub_old_streams, stream);
            }
            else {
                JANUS_LOG(LOG_VERB, "Removing PubSub subscriber...\n");
                if (session->sub_id > 0) {
                    janus_mutex_lock(&stream->subscribers_mutex);
                    janus_pubsub_stream *subscriber = g_hash_table_lookup(stream->subscribers, &session->sub_id);
                    if (!subscriber || subscriber->destroyed) {
                         JANUS_LOG(LOG_ERR, "Subscribers hashtable lookup failed...\n");
                         *error = -2;
                         janus_mutex_unlock(&stream->subscribers_mutex);
                         janus_mutex_unlock(&pubsub_streams_mutex);
                         janus_mutex_unlock(&pubsub_sessions_mutex);
                         return;
                    }
                    g_hash_table_remove(stream->subscribers, &session->sub_id);
                    subscriber->destroyed = janus_get_monotonic_time();
	            pubsub_old_subscribers = g_list_append(pubsub_old_subscribers, subscriber);
                    janus_mutex_unlock(&stream->subscribers_mutex);
                }
            }
        }
        JANUS_LOG(LOG_INFO, "Set session destroyed...\n");
        session->destroyed = janus_get_monotonic_time();
        g_hash_table_remove(sessions, handle);
        /* Cleaning up and removing the session is done in a lazy way */
        pubsub_old_sessions = g_list_append(pubsub_old_sessions, session);
        janus_mutex_unlock(&pubsub_streams_mutex);
    }
    janus_mutex_unlock(&pubsub_sessions_mutex);
    JANUS_LOG(LOG_INFO, "PubSub Session destroyed.\n");
}
