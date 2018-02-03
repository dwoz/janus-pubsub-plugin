#ifndef COMMON_H
#define COMMON_H
#include <glib.h>
#include <netdb.h>


typedef struct janus_pubsub_puller {
    gboolean is_video;
    gboolean is_data;
    int pull_sock;                      /* The udp socket to stream rtp packets from */
    uint32_t ssrc;
    int payload_type;
    struct sockaddr_in serv_addr;
} janus_pubsub_puller;


int create_puller(janus_pubsub_puller *puller);
int destroy_puller(janus_pubsub_puller *puller);

#endif /* COMMON_H */
