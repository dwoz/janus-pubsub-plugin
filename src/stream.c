#include <glib.h>
#include "stream.h"

static GHashTable *streams;

void janus_pubsub_streams_init(void) {
    streams = g_hash_table_new(g_str_hash, g_str_equal);
}

janus_pubsub_stream * janus_pubsub_stream_get(gchar *name){
    janus_pubsub_stream * s = g_hash_table_lookup(streams, name);
    return s;
}

int janus_pubsub_add_stream(janus_pubsub_stream *stream) {
    g_hash_table_insert(streams, g_strdup(stream->name), stream);
}
gboolean janus_pubsub_has_stream(gchar *name) {
    return g_hash_table_contains(streams, name);
}

int janus_pubsub_create_stream(janus_pubsub_stream **stream_p)
{
    janus_pubsub_stream *stream = g_malloc0(sizeof(janus_pubsub_stream));
    stream->pub_id = 0;
    stream->name = NULL;
    stream->kind = -1;
    stream->sdp = NULL;
    stream->sdp_type = NULL;
    stream->video_port = 0;
    stream->audio_port = 0;
    stream->data_port = 0;
    stream->host = NULL;
    stream->fwd_sock = 0;
    stream->relay_thread = NULL;
    stream->publisher = NULL;
    stream->subscribers = g_hash_table_new(NULL, NULL);
    janus_mutex_init(&stream->subscribers_mutex);
    stream->video_puller = NULL;
    stream->audio_puller = NULL;
    stream->data_puller = NULL;
    stream->destroyed = 0;
    stream->relay_rtp = NULL;
    *stream_p = stream;
    return 0;
}


int janus_pubsub_destroy_stream(janus_pubsub_stream *stream)
{
    g_hash_table_destroy(stream->subscribers);
    janus_mutex_destroy(&stream->subscribers_mutex);
    g_free(stream);
    stream = NULL;
    return 0;
}
