#include "campfire.h"
#include "notify.h"
#include <xmlnode.h>
#include <glib/gi18n.h>

void campfire_room_query(CampfireConn *campfire);

void campfire_plugin_init(PurplePlugin *plugin)
{
	/*
	purple_notify_message(plugin, PURPLE_NOTIFY_MSG_INFO, "Hello World!",
                        "This is the Hello World! plugin :)", NULL, NULL, NULL);
	*/
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

GList *campfire_chat_info(PurpleConnection *gc)
{
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("_Room:");
	pce->identifier = "room";
	pce->required = TRUE;
	m = g_list_append(m, pce);

	return m;
}

PurpleRoomlist *campfire_roomlist_get_list(PurpleConnection *gc)
{	
	CampfireConn *campfire = gc->proto_data;
	GList *fields = NULL;
	PurpleRoomlistField *f;

	if (campfire->roomlist)
		purple_roomlist_unref(campfire->roomlist);
	
	campfire->roomlist = purple_roomlist_new(purple_connection_get_account(gc));

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", "room", TRUE);
	fields = g_list_append(fields, f);

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Topic"), "topic", FALSE);
	fields = g_list_append(fields, f);

	purple_roomlist_set_fields(campfire->roomlist, fields);

	campfire_room_query(campfire);
	
	return campfire->roomlist;
}

void campfire_roomlist_cancel(PurpleRoomlist *list)
{
	PurpleConnection *gc = purple_account_get_connection(list->account);

	if (gc == NULL)
		return;

	CampfireConn *campfire = gc->proto_data;

	purple_roomlist_set_in_progress(list, FALSE);

	if (campfire->roomlist == list) {
		campfire->roomlist = NULL;
		purple_roomlist_unref(list);
	}
}

void campfire_room_query(CampfireConn *campfire)
{
	purple_roomlist_set_in_progress(campfire->roomlist, TRUE);
	//@TODO do some curl/xml stuff here
	//see
	//protocols/jabber/chat.c:864 roomlist_ok_cb() AND
	//protocols/jabber/chat.c:800 roomlist_disco_result_cb()
	purple_roomlist_set_in_progress(campfire->roomlist, FALSE);
	purple_roomlist_unref(campfire->roomlist);
	campfire->roomlist = NULL;
}
