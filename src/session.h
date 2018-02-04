#ifndef SESSION_H
#define SESSION_H
#include <glib.h>

/* janus includes */
#include <plugins/plugin.h>
#include <mutex.h>
#include <record.h>

typedef struct janus_pubsub_session {
    janus_plugin_session *handle;
    gchar *stream_name;                /* session publishes to this stream */
    guint64 sub_id;                    /* subscriber id */
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
    int kind;
    gint64 destroyed;    /* Time at which this session was marked as destroyed */
} janus_pubsub_session;

void janus_pubsub_sessions_init(void);
void janus_pubsub_sessions_destroy(void);
janus_pubsub_session *janus_pubsub_session_get(janus_plugin_session *handle);
gboolean janus_pubsub_has_session(janus_plugin_session *handle);
void janus_pubsub_create_session(janus_plugin_session *handle, int *error);
void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error);

#endif /* SESSION_H */
