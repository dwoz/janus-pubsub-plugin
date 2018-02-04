#ifndef PUBSUB_H

#define PUBSUB_H

#include <glib.h>

/* janus includes */
#include <plugins/plugin.h>
#include <mutex.h>

/* Plugin config defaults */
#define PUBSUB_DEFAULT_PUB_URL "http://localhost:5000/publish"
#define PUBSUB_DEFAULT_SUB_URL "http://localhost:5000/play"
#define PUBSUB_DEFAULT_FWD_HOST "127.0.0.1"
#define PUBSUB_DEFAULT_PULL_HOST "127.0.0.1"


/* Error codes */
#define JANUS_PUBSUB_ERROR_NO_MESSAGE         411
#define JANUS_PUBSUB_ERROR_INVALID_JSON       412
#define JANUS_PUBSUB_ERROR_INVALID_ELEMENT    413
#define JANUS_PUBSUB_ERROR_INVALID_SDP        414
#define JANUS_PUBSUB_ERROR_MISSING_ELEMENT    429
#define JANUS_PUBSUB_ERROR_UNKNOWN_ERROR      499


/* Stream Kinds */
#define JANUS_PUBTYP_SESSION     1
#define JANUS_PUBTYP_PULL        2

/* Subscriber Kinds */
#define JANUS_SUBTYP_SESSION     1
#define JANUS_SUBTYP_FORWARD     2


/* Session Kinds */
#define JANUS_SESSION_NONE 0
#define JANUS_SESSION_SUBSCRIBE 1
#define JANUS_SESSION_PUBLISH 2
extern janus_mutex pubsub_streams_mutex;
extern janus_mutex pubsub_sessions_mutex;

extern GHashTable *pubsub_streams;
extern GList *pubsub_old_sessions;
extern GList *pubsub_old_streams;
extern GList *pubsub_old_subscribers;


int janus_pubsub_is_initialized(void);
int janus_pubsub_is_stopping(void);
#endif /* PUBSUB_H */
