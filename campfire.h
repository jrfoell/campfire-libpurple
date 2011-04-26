#ifndef CAMPFIRE_H
#define CAMPFIRE_H
#include "plugin.h"

void campfire_plugin_init(PurplePlugin *plugin);

typedef struct _CampfireMessage {
	enum {
		CAMPFIRE_MESSAGE_TEXT,
		CAMPFIRE_MESSAGE_PASTE,
		CAMPFIRE_MESSAGE_SOUND,
		CAMPFIRE_MESSAGE_TWEET
	} type;
	char *body;
} CampfireMessage;


#endif /* CAMPFIRE_H */

