#ifndef FORWARD_H
#define FORWARD_H

#include <glib.h>
#include <netinet/in.h>

typedef struct janus_pubsub_forwarder {
    gboolean is_video;
    gboolean is_data;
    uint32_t ssrc;
    int payload_type;
    struct sockaddr_in serv_addr;
} janus_pubsub_forwarder;

#endif /* FORWARD_H */
