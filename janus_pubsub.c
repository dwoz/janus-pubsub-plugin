#include <jansson.h>
#include <plugins/plugin.h>
#include <debug.h>
#include <config.h>

#define JANUS_PUBSUB_VERSION 1
#define JANUS_PUBSUB_VERSION_STRING	"0.0.1"
#define JANUS_PUBSUB_DESCRIPTION "The simplest possible Janus plugin."
#define JANUS_PUBSUB_NAME "JANUS pub sub plugin"
#define JANUS_PUBSUB_AUTHOR	"Marshall Quander"
#define JANUS_PUBSUB_PACKAGE "janus.plugin.pubsub"

janus_plugin *create(void);
int janus_pubsub_init(janus_callbacks *callback, const char *config_path);
void janus_pubsub_destroy(void);
int janus_pubsub_get_api_compatibility(void);
int janus_pubsub_get_version(void);
const char *janus_pubsub_get_version_string(void);
const char *janus_pubsub_get_description(void);
const char *janus_pubsub_get_name(void);
const char *janus_pubsub_get_author(void);
const char *janus_pubsub_get_package(void);
void janus_pubsub_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_pubsub_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_pubsub_setup_media(janus_plugin_session *handle);
void janus_pubsub_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_pubsub_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len);
void janus_pubsub_incoming_data(janus_plugin_session *handle, char *buf, int len);
void janus_pubsub_slow_link(janus_plugin_session *handle, int uplink, int video);
void janus_pubsub_hangup_media(janus_plugin_session *handle);
void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error);
json_t *janus_pubsub_query_session(janus_plugin_session *handle);

static janus_plugin janus_pubsub_plugin =
	JANUS_PLUGIN_INIT (
		.init = janus_pubsub_init,
		.destroy = janus_pubsub_destroy,

		.get_api_compatibility = janus_pubsub_get_api_compatibility,
		.get_version = janus_pubsub_get_version,
		.get_version_string = janus_pubsub_get_version_string,
		.get_description = janus_pubsub_get_description,
		.get_name = janus_pubsub_get_name,
		.get_author = janus_pubsub_get_author,
		.get_package = janus_pubsub_get_package,

		.create_session = janus_pubsub_create_session,
		.handle_message = janus_pubsub_handle_message,
		.setup_media = janus_pubsub_setup_media,
		.incoming_rtp = janus_pubsub_incoming_rtp,
		.incoming_rtcp = janus_pubsub_incoming_rtcp,
		.incoming_data = janus_pubsub_incoming_data,
		.slow_link = janus_pubsub_slow_link,
		.hangup_media = janus_pubsub_hangup_media,
		.destroy_session = janus_pubsub_destroy_session,
		.query_session = janus_pubsub_query_session,
	);


static gboolean notify_events = TRUE;

janus_plugin *create(void) {
	JANUS_LOG(LOG_VERB, "%s created!\n", JANUS_PUBSUB_NAME);
	return &janus_pubsub_plugin;
}

int janus_pubsub_init(janus_callbacks *callback, const char *config_path) {
	if(callback == NULL || config_path == NULL) {
		/* Invalid arguments */
		return -1;
	}

	/* Read configuration */
	char filename[255];
	g_snprintf(filename, 255, "%s/%s.cfg", config_path, JANUS_PUBSUB_PACKAGE);
	JANUS_LOG(LOG_VERB, "Configuration file: %s\n", filename);
	janus_config *config = janus_config_parse(filename);
	if(config != NULL) {
		janus_config_print(config);
		janus_config_item *events = janus_config_get_item_drilldown(config, "general", "events");
		if(events != NULL && events->value != NULL)
			notify_events = janus_is_true(events->value);
		if(!notify_events && callback->events_is_enabled()) {
			JANUS_LOG(LOG_WARN, "Notification of events to handlers disabled for %s\n", JANUS_PUBSUB_NAME);
		}
	}
	janus_config_destroy(config);
	config = NULL;
	JANUS_LOG(LOG_INFO, "%s initialized!\n", JANUS_PUBSUB_NAME);
	return 0;
}


void janus_pubsub_destroy(void) {
	JANUS_LOG(LOG_INFO, "%s destroyed!\n", JANUS_PUBSUB_NAME);
}

int janus_pubsub_get_api_compatibility(void) {
	return JANUS_PLUGIN_API_VERSION;
}

int janus_pubsub_get_version(void) {
	return JANUS_PUBSUB_VERSION;
}

const char *janus_pubsub_get_version_string(void) {
	return JANUS_PUBSUB_VERSION_STRING;
}

const char *janus_pubsub_get_description(void) {
	return JANUS_PUBSUB_DESCRIPTION;
}

const char *janus_pubsub_get_name(void) {
	return JANUS_PUBSUB_NAME;
}

const char *janus_pubsub_get_author(void) {
	return JANUS_PUBSUB_AUTHOR;
}

const char *janus_pubsub_get_package(void) {
	return JANUS_PUBSUB_PACKAGE;
}


void janus_pubsub_create_session(janus_plugin_session *handle, int *error) {
	JANUS_LOG(LOG_INFO, "Session created.\n");
}

void janus_pubsub_destroy_session(janus_plugin_session *handle, int *error) {
	JANUS_LOG(LOG_INFO, "Session destroyed.\n");
}

json_t *janus_pubsub_query_session(janus_plugin_session *handle) {
	return json_object();
}

struct janus_plugin_result *janus_pubsub_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {
	return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, json_object());
}

void janus_pubsub_setup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "WebRTC media is now available.\n");
}

void janus_pubsub_incoming_rtp(janus_plugin_session *handle, int video, char *buf, int len) {
	JANUS_LOG(LOG_VERB, "Got an RTP message (%d bytes.)\n", len);
}

void janus_pubsub_incoming_rtcp(janus_plugin_session *handle, int video, char *buf, int len) {
	JANUS_LOG(LOG_VERB, "Got an RTCP message (%d bytes.)\n", len);
}

void janus_pubsub_incoming_data(janus_plugin_session *handle, char *buf, int len) {
	JANUS_LOG(LOG_VERB, "Got a DataChannel message (%d bytes.)\n", len);
}

void janus_pubsub_slow_link(janus_plugin_session *handle, int uplink, int video) {
	JANUS_LOG(LOG_VERB, "Slow link detected.\n");
}

void janus_pubsub_hangup_media(janus_plugin_session *handle) {
	JANUS_LOG(LOG_INFO, "No WebRTC media anymore.\n");
}
