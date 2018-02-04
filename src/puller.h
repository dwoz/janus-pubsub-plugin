#ifndef PULLER_H
#define PULLER_H

#include <glib.h>
#include <netinet/in.h>

typedef struct janus_pubsub_puller {
    gboolean is_video;
    gboolean is_data;
    int pull_sock;                      /* The udp socket to stream rtp packets from */
    uint32_t ssrc;
    int payload_type;
    struct sockaddr_in serv_addr;
} janus_pubsub_puller;


#endif /* PULLER_H */
