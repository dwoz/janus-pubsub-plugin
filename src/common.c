#include <glib.h>
#include <netdb.h>
#include "common.h"


int create_puller(janus_pubsub_puller *puller)
{
    puller = g_malloc0(sizeof(janus_pubsub_puller));
    puller->is_video = FALSE;
    puller->is_data = FALSE;
    puller->pull_sock = 0;
    puller->ssrc = 0;
    puller->payload_type = 0;
    puller->pull_sock = 0;
    return 0;
}

int destroy_puller(janus_pubsub_puller *puller)
{
   g_free(puller);
   puller = NULL;
   return 0;
}
