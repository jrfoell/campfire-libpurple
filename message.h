#ifndef MESSAGE_H
#define MESSAGE_H
#include "plugin.h"

typedef struct _CampfireMessage {
	enum {
		CAMPFIRE_MESSAGE_TEXT,
		CAMPFIRE_MESSAGE_PASTE,
		CAMPFIRE_MESSAGE_SOUND,
		CAMPFIRE_MESSAGE_TWEET
	} type;
	char *body;
} CampfireMessage;


#endif /* not MESSAGE_H */

