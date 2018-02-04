#ifndef STREAM_H
#define STREAM_H

#include <glib.h>

/* janus includes */
#include <plugins/plugin.h>
#include <mutex.h>

#include "puller.h"
#include "session.h"

typedef struct jansus_pubsub_stream {
    guint64 pub_id;                    /* Unique Publisher ID */
    gchar *name;                       /* Unique name given to this pubslisher */
    int kind;                          /* Type of publisher */
    gchar *sdp;                        /* The SDP this publisher negotiated, if any */
    gchar *sdp_type;
    int video_port;
    int audio_port;
    int data_port;
    char *host;
    int fwd_sock;                      /* The udp socket on which to forward rtp packets */
    GThread *relay_thread;
    janus_pubsub_session *publisher;
    janus_mutex subscribers_mutex;
    GHashTable *subscribers;
    janus_pubsub_puller* video_puller;
    janus_pubsub_puller* audio_puller;
    janus_pubsub_puller* data_puller;
    gint64 destroyed;                  /* Time at which this stream was marked as destroyed */
    void (*relay_rtp)(void *stream, int video, char *buf, int len);
} janus_pubsub_stream;

void janus_pubsub_streams_init(void);
janus_pubsub_stream * janus_pubsub_stream_get(gchar *name);
int janus_pubsub_create_stream(janus_pubsub_stream **stream_p);
int janus_pubsub_destroy_stream(janus_pubsub_stream *stream);

#endif /* STREAM_H */
