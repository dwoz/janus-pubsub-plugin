#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <glib.h>

#include <plugins/plugin.h>

typedef struct janus_pubsub_subscriber {
    guint64 subscriber_id;             /* Unique Subscriber ID */
    int kind;
    char *host;
    int video_port;
    int audio_port;
    int data_port;
    GHashTable *rtp_forwarders;
    janus_mutex rtp_forwarders_mutex;
    janus_pubsub_session *subscriber_session;
    gint64 destroyed;                 /* Time at which this stream was marked as destroyed */
} janus_pubsub_subscriber;


#endif /* SUBSCRIBER_H */
