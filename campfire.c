#include "campfire.h"
#include "notify.h"
#include <xmlnode.h>

void campfire_plugin_init(PurplePlugin *plugin)
{
	purple_notify_message(plugin, PURPLE_NOTIFY_MSG_INFO, "Hello World!",
                        "This is the Hello World! plugin :)", NULL, NULL, NULL);
}

void campfire_message_send(CampfireMessage *cm)
{
	xmlnode *message, *child;
	const char *type = NULL;

	message = xmlnode_new("message");

	switch(cm->type) {
		case CAMPFIRE_MESSAGE_PASTE:
			type = "PasteMessage";
			break;
		case CAMPFIRE_MESSAGE_SOUND:
			type = "SoundMessage";
			break;
		case CAMPFIRE_MESSAGE_TWEET:
			type = "TweetMessage";
			break;
		case CAMPFIRE_MESSAGE_TEXT:
		default:
			type = "TextMessage";
			break;
	}

	if(type)
		xmlnode_set_attrib(message, "type", type);
	
	if(cm->body) {
		child = xmlnode_new_child(message, "body");
		xmlnode_insert_data(child, cm->body, -1);
	}
	
	//jabber_send(message);

	xmlnode_free(message);
}
