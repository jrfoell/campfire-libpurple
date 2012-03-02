//local includes
#include "message.h"

//purple includes
#include <debug.h>

//internal function prototype
void campfire_print_messages(CampfireSslTransaction *xaction, PurpleSslConnection *gsc, PurpleInputCondition cond);

CampfireMessage * campfire_get_message(xmlnode *xmlmessage)
{
	xmlnode *xmlbody = NULL, *xmluser_id = NULL, *xmltime = NULL, *xmltype = NULL, *xmlid = NULL;
	gchar *body = NULL, *user_id = NULL, *msgtype = NULL, *msg_id = NULL;
	CampfireMessage *msg = NULL;
	GTimeVal timeval;
	time_t mtime;
	
	xmlbody = xmlnode_get_child(xmlmessage, "body");
	body = xmlnode_get_data(xmlbody);
		
	xmluser_id = xmlnode_get_child(xmlmessage, "user-id");
	user_id = xmlnode_get_data(xmluser_id);

	xmltime = xmlnode_get_child(xmlmessage, "created-at");
	if(g_time_val_from_iso8601(xmlnode_get_data(xmltime), &timeval))
	{
		mtime = timeval.tv_sec;
	}
								
	xmltype = xmlnode_get_child(xmlmessage, "type");
	msgtype = xmlnode_get_data(xmltype);

	xmlid = xmlnode_get_child(xmlmessage, "id");
	msg_id = xmlnode_get_data(xmlid);

	if (g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TIME) == 0)
	{
		purple_debug_info("campfire", "Skipping message of type: %s\n", msgtype);
		xmlmessage = xmlnode_get_next_twin(xmlmessage);
		return NULL;
	}
				
	purple_debug_info("campfire", "got message of type: %s\n", msgtype);
	msg = g_new0(CampfireMessage, 1);
	msg->id = msg_id;
	msg->user_id = user_id;
	msg->time = mtime;
	msg->type = msgtype;
		
	if (g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TEXT) == 0 ||
		g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TWEET) == 0 ||
		g_strcmp0(msgtype, CAMPFIRE_MESSAGE_PASTE) == 0 ||
		g_strcmp0(msgtype, CAMPFIRE_MESSAGE_SOUND) == 0 ||
		g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TOPIC) == 0)
	{
		msg->message = body;
	}
	return msg;
}

void campfire_userlist_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                                PurpleInputCondition cond)
{
	PurpleConversation *convo = NULL;
	xmlnode *xmlroomname = NULL, *xmltopic = NULL, *xmlusers = NULL, *xmluser = NULL, *xmlname = NULL;
	gchar *room_name = NULL, *topic = NULL, *name = NULL;
	GList *users = NULL, *chatusers = NULL;
	PurpleConvChatBuddy *buddy = NULL;
	gboolean found;
	
	xmlroomname = xmlnode_get_child(xaction->xml_response, "name");
	room_name = xmlnode_get_data(xmlroomname);
	purple_debug_info("campfire", "locating room: %s\n", room_name);
	convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_name, purple_connection_get_account(xaction->campfire->gc));

	xmltopic = xmlnode_get_child(xaction->xml_response, "topic");
	topic = xmlnode_get_data(xmltopic);
	purple_debug_info("campfire", "setting topic to %s\n", topic);
	purple_conv_chat_set_topic(PURPLE_CONV_CHAT(convo), NULL, topic);
	xmlusers = xmlnode_get_child(xaction->xml_response, "users");
	xmluser = xmlnode_get_child(xmlusers, "user");

	while (xmluser != NULL)
	{
		xmlname = xmlnode_get_child(xmluser, "name");
		name = xmlnode_get_data(xmlname);
		purple_debug_info("campfire", "user in room: %s\n", name);

		if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(convo), name))
		{
			purple_debug_info("campfire", "adding user %s to room\n", name);
			purple_conv_chat_add_user(PURPLE_CONV_CHAT(convo), name, NULL, PURPLE_CBFLAGS_NONE, TRUE);
			purple_debug_info("campfire", "not crashed yet\n");
		}
		users = g_list_prepend(users, name);
		purple_debug_info("campfire", "not crashed yet 2\n");
		xmluser = xmlnode_get_next_twin(xmluser);
	}

	purple_debug_info("campfire", "Getting all users in room\n");
	chatusers = purple_conv_chat_get_users(PURPLE_CONV_CHAT(convo));
	purple_debug_info("campfire", "got all users in room %p\n", chatusers);

	if (users == NULL) //probably shouldn't happen
	{
		purple_debug_info("campfire", "removing all users from room\n");
		purple_conv_chat_remove_users(PURPLE_CONV_CHAT(convo), chatusers, NULL);
	}
	else if (chatusers != NULL) //also probably shouldn't happen
	{
		purple_debug_info("campfire", "iterating chat users\n");
		for (; chatusers != NULL; chatusers = chatusers->next)
		{
			buddy = chatusers->data;
			found = FALSE;
			purple_debug_info("campfire", "checking to see if user %s has left\n", buddy->name);
			for (; users; users = users->next)
			{
				if (g_strcmp0(users->data, buddy->name) == 0)
				{
					purple_debug_info("campfire", "user %s is still here\n", buddy->name);
					found = TRUE;
					break;
				}
			}

			if (!found)
			{
				purple_debug_info("campfire", "removing user %s that has left\n", buddy->name);
				purple_conv_chat_remove_user(PURPLE_CONV_CHAT(convo), buddy->name, NULL);
			}
			
			//g_free(c->data);
		}

		//g_list_free(c);
		//g_list_free(users);			
	}

	//g_list_free(chatusers);
	
}

void campfire_message_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                               PurpleInputCondition cond)
{
	xmlnode *xmlmessage = NULL;	
	GList *msgs = NULL;
	CampfireMessage *msg = NULL;
	CampfireSslTransaction *xaction2 = NULL;
	
	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	xmlmessage = xmlnode_get_child(xaction->xml_response, "message");

	while (xmlmessage != NULL)
	{
		msg = campfire_get_message(xmlmessage);
		if(msg)
			msgs = g_list_append(msgs, msg);
		
		xmlmessage = xmlnode_get_next_twin(xmlmessage);
	}

	//print the messages out
	purple_debug_info("campfire", "calling print messages\n");
				
	//create a new xaction
	xaction2 = g_new0(CampfireSslTransaction, 1);
	xaction2->campfire = xaction->campfire;
	xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
	xaction2->response_cb_data = xaction2;
	xaction2->messages = msgs;
	xaction2->room_id = g_strdup(xaction->room_id);
	xaction2->first_check = xaction->first_check;

	campfire_print_messages(xaction2, xaction->campfire->gsc, PURPLE_INPUT_READ);
}

gboolean campfire_room_check(CampfireConn *campfire)	
{
	GList *rooms = g_hash_table_get_values(campfire->rooms);
	CampfireRoom *room = NULL;
	CampfireSslTransaction *xaction = NULL, *xaction2 = NULL;
	GString *uri = NULL;
	
	// cancel the timer if we've left all rooms
	if(!rooms)
	{
		purple_debug_info("campfire", "not in any rooms, removing timer\n");
		campfire->message_timer = 0;
		return FALSE;
	}
	
	for (; rooms != NULL; rooms = rooms->next)
	{
		room = rooms->data;

		//first check the room users
		xaction = g_new0(CampfireSslTransaction, 1);
		xaction->campfire = campfire;
		xaction->response_cb = (PurpleSslInputFunction)campfire_userlist_callback;
		xaction->response_cb_data = xaction;
	
		purple_debug_info("campfire", "checking for users in room: %s\n", room->id);

		uri = g_string_new("/room/");
		g_string_append(uri, room->id);
		g_string_append(uri, ".xml");
		
		campfire_http_request(xaction, uri->str, "GET", NULL);
		campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
		g_string_free(uri, TRUE);

		//then get recent messages
		if(room->last_message_id)
		{
			xaction2 = g_new0(CampfireSslTransaction, 1);
			xaction2->campfire = campfire;
			xaction2->response_cb = (PurpleSslInputFunction)campfire_message_callback;
			xaction2->response_cb_data = xaction2;
			xaction2->room_id = g_strdup(room->id);

			uri = g_string_new("/room/");
			g_string_append(uri, room->id);
			g_string_append(uri, "/recent.xml?since_message_id=");
			g_string_append(uri, room->last_message_id);

			purple_debug_info("campfire", "getting latest messages: %s\n", uri->str);

			campfire_http_request(xaction2, uri->str, "GET", NULL);
			g_string_free(uri, TRUE);
			campfire_queue_xaction(campfire, xaction2, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
		}
	}

	//@TODO set this to zero once all rooms have been left
	if(!campfire->message_timer)
	{
		//call this function again periodically to check for new users
		campfire->message_timer = purple_timeout_add_seconds(3, (GSourceFunc)campfire_room_check, campfire);
	}

	return TRUE;
}

/** also functions as a callback (to itself) */
void campfire_print_messages(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireConn *campfire = xaction->campfire;
	GList *first = g_list_first(xaction->messages);
	gboolean print = TRUE;
	xmlnode *xmlusername = NULL, *xmluserid = NULL, *xmlurl = NULL;
	gchar *user_id = NULL, *username = NULL, *user_name = NULL, *msg_id = NULL, *upload_url = NULL;
	CampfireMessage *msg = NULL;
	GString *uri, *message = NULL;
	CampfireSslTransaction *xaction2 = NULL;
	CampfireRoom *room = NULL;
	PurpleConversation *convo = NULL;

	purple_debug_info("campfire", "%s first_check:%s my_message:%s\n",
					  __FUNCTION__,
					  xaction->first_check?"true":"false",
					  xaction->my_message?"true":"false");
		
	//initialize user list
	if(!campfire->users)
	{
		campfire->users = g_hash_table_new(g_str_hash, g_str_equal);
	}

	//handle possible response(s) (if used as a callback)
	if(xaction->xml_response)
	{
		purple_debug_info("campfire", "got xml %s\n", xmlnode_to_str(xaction->xml_response, NULL));
		xmlurl = xmlnode_get_child(xaction->xml_response, "full-url");
		if(xmlurl) //it's an upload response
		{
			upload_url = xmlnode_get_data(xmlurl);
			purple_debug_info("campfire", "got upload URL %s\n", upload_url);
		}
		else //it's a user response
		{
			xmlusername = xmlnode_get_child(xaction->xml_response, "name");
			xmluserid = xmlnode_get_child(xaction->xml_response, "id");

			user_id = xmlnode_get_data(xmluserid);
			username = xmlnode_get_data(xmlusername);
			purple_debug_info("campfire", "adding username %s ID %s\n", username, user_id);
			g_hash_table_replace(campfire->users, user_id, username);			
		}		
	}
	
	if (!first) {
		//print the user as "in the room" after the previous messages have been printed
		//(only if this is the first message check)
		if(xaction->first_check)
			campfire_room_check(campfire);
		
		//maybe cleanup here?
		purple_debug_info("campfire", "no more messages to process\n");
	} else {
		msg = first->data;

		user_name = g_hash_table_lookup(campfire->users, msg->user_id);
		
		purple_debug_info("campfire", "Looked up user_id: %s, got %s\n", msg->user_id, user_name);

		if(!user_name)
		{
			uri = g_string_new("/users/");
			g_string_append(uri, msg->user_id);
			g_string_append(uri, ".xml");
			
			//create a new one
			xaction2 = g_new0(CampfireSslTransaction, 1);

			xaction2->campfire = xaction->campfire;
			xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
			xaction2->response_cb_data = xaction2;
			xaction2->messages = xaction->messages;
			xaction2->room_id = g_strdup(xaction->room_id);
			xaction2->first_check = xaction->first_check;
			xaction2->my_message = xaction->my_message;
			
			campfire_http_request(xaction2, uri->str, "GET", NULL);
			campfire_queue_xaction(campfire, xaction2, cond);
		} else {

			
			//print
			room = g_hash_table_lookup(campfire->rooms, xaction->room_id);
			convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room->name, purple_connection_get_account(xaction->campfire->gc));

			purple_debug_info("campfire", "Writing message ID \"%s\" type \"%s\" to %p from name \"%s\"\n", msg->id, msg->type, convo, user_name);

			// some explanation: when you send a message, the resulting
			// message gets sent back as a confirmation. The
			// confirmation message winds up here to be printed out.
			//
			// In order to not print out your own messages twice from
			// the recent.xml?since_message_id=X request, we must hint
			// this function when we're being called from our own message send
			// (xaction->my_message) and skip our own messages when we
			// do a period message print out.
			if(!xaction->my_message) 
			{
				for (; room->my_message_ids != NULL; room->my_message_ids = room->my_message_ids->next)
				{
					msg_id = room->my_message_ids->data;
					if(g_strcmp0(msg_id, msg->id) == 0)
					{
						purple_debug_info("campfire", "Won't write message \"%s\" it was mine\n", msg_id);						
						print = FALSE;
						room->my_message_ids = g_list_remove(room->my_message_ids, msg_id);
						break;
					}
				}
			}

			if(print)
			{
				if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_UPLOAD) == 0 && !upload_url)
				{
					purple_debug_info("campfire", "Going to fetch upload\n");
					uri = g_string_new("/room/");
					g_string_append(uri, xaction->room_id);
					g_string_append(uri, "/messages/");
					g_string_append(uri, msg->id);
					g_string_append(uri, "/upload.xml");

					xaction2 = g_new0(CampfireSslTransaction, 1);

					xaction2->campfire = xaction->campfire;
					xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
					xaction2->response_cb_data = xaction2;
					xaction2->messages = xaction->messages;
					xaction2->room_id = g_strdup(xaction->room_id);
					xaction2->first_check = xaction->first_check;
					xaction2->my_message = xaction->my_message;
			
					campfire_http_request(xaction2, uri->str, "GET", NULL);
					campfire_queue_xaction(campfire, xaction2, cond);
					//return here so we can print this message out
					//again once we've retrieved the upload info
					return;
				}
				else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TEXT) == 0 ||
						g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TWEET) == 0 ||
						g_strcmp0(msg->type, CAMPFIRE_MESSAGE_PASTE) == 0)
				{
					purple_debug_info("campfire", "Writing chat message \"%s\" to %p from name %s\n", msg->message, convo, user_name);
					purple_conversation_write(convo, user_name, msg->message,
											  PURPLE_MESSAGE_RECV,
											  msg->time);
				}
				else
				{
					message = g_string_new(user_name);
					if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_ENTER) == 0)
					{
						g_string_append(message, " has entered the room.");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_LEAVE) == 0)
					{
						g_string_append(message, " has left the room.");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_KICK) == 0)
					{
						g_string_append(message, " kicked.");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_GUESTALLOW) == 0)
					{
						g_string_append(message, " turned on guest access.");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_GUESTDENY) == 0)
					{
						g_string_append(message, " turned off guest access.");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TOPIC) == 0)
					{
						g_string_append(message, " changed the room's topic to \"");
						g_string_append(message, msg->message);
						g_string_append(message, "\"");
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_UPLOAD) == 0 && upload_url)
					{
						g_string_append(message, " uploaded ");
						g_string_append(message, upload_url);
					}
					else if(g_strcmp0(msg->type, CAMPFIRE_MESSAGE_SOUND) == 0)
					{
						g_string_append(message, " sounded off https://");
						g_string_append(message, campfire->hostname);							
						g_string_append(message, "/sounds/");
						g_string_append(message, msg->message);	
						g_string_append(message, ".mp3");					
					}
					purple_conversation_write(convo, "", message->str,
											  PURPLE_MESSAGE_SYSTEM,
											  msg->time);
				}
			}
			
			//remember the last message we've written
			if(!xaction->my_message)
				room->last_message_id = msg->id;
			
			xaction->messages = g_list_remove(xaction->messages, msg);
			xaction->xml_response = NULL;
			//recurse
			campfire_print_messages(xaction, gsc, cond);
		}
	}
}

void campfire_message_send_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
									PurpleInputCondition cond)
{
	CampfireRoom *room = g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);	
	CampfireMessage *msg = campfire_get_message(xaction->xml_response);
	GList *msgs = NULL;
	CampfireSslTransaction *xaction2 = NULL;

	//pretty much the same as campfire_message_callback
	//but this one only processes one message
	purple_debug_info("campfire", "%s\n", __FUNCTION__);
	
	msgs = g_list_append(msgs, msg);

	//save this message id so we don't set it as last_message_id (or re-print it)
	if(!room->my_message_ids)
	{
		room->my_message_ids = g_list_append(room->my_message_ids, msg->id);
	}

	//print the messages out
	purple_debug_info("campfire", "calling print messages\n");
				
	//create a new xaction
	xaction2 = g_new0(CampfireSslTransaction, 1);
	xaction2->campfire = xaction->campfire;
	xaction2->response_cb = (PurpleSslInputFunction)campfire_print_messages;
	xaction2->response_cb_data = xaction2;
	xaction2->messages = msgs;
	xaction2->my_message = TRUE; //let campfire_print_messages know where we're coming from
	xaction2->room_id = g_strdup(xaction->room_id);
	
	campfire_print_messages(xaction2, xaction->campfire->gsc, PURPLE_INPUT_READ);
}

void campfire_message_send(CampfireConn *campfire, int id, const char *message, char *msg_type)
{	
	gchar *room_id = g_strdup_printf("%i", id);
	xmlnode *xmlmessage = NULL, *xmlchild = NULL;
	CampfireSslTransaction *xaction = NULL;
	GString *uri = NULL;

	if(!msg_type)
		msg_type = CAMPFIRE_MESSAGE_TEXT;
	
	xmlmessage = xmlnode_new("message");
	xmlnode_set_attrib(xmlmessage, "type", msg_type);	
	xmlchild = xmlnode_new_child(xmlmessage, "body");
	xmlnode_insert_data(xmlchild, message, -1);
	
	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/speak.xml");

	xaction = g_new0(CampfireSslTransaction, 1);
	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_message_send_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);

	purple_debug_info("campfire", "Sending message %s\n", xmlnode_to_str(xmlmessage, NULL));
		
	campfire_http_request(xaction, uri->str, "POST", xmlmessage);
	g_string_free(uri, TRUE);
	g_free(room_id);
	xmlnode_free(xmlmessage);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}

void campfire_room_query_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                                  PurpleInputCondition cond)
{
	xmlnode *xmlroom = NULL, *xmlname = NULL, *xmltopic = NULL, *xmlid = NULL;
	gchar *name = NULL, *topic = NULL, *id = NULL;
	PurpleRoomlistRoom *room = NULL;

	purple_debug_info("campfire", "processing xml...\n");
	xmlroom = xmlnode_get_child(xaction->xml_response, "room");
	while (xmlroom != NULL)
	{
		xmlname = xmlnode_get_child(xmlroom, "name");
		name = xmlnode_get_data(xmlname);
		xmltopic = xmlnode_get_child(xmlroom, "topic");
		topic = xmlnode_get_data(xmltopic);
		xmlid = xmlnode_get_child(xmlroom, "id");
		id = xmlnode_get_data(xmlid);
	
		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
		purple_roomlist_room_add_field(xaction->campfire->roomlist, room, topic);
		purple_roomlist_room_add_field(xaction->campfire->roomlist, room, id);
		purple_roomlist_room_add(xaction->campfire->roomlist, room);
		xmlroom = xmlnode_get_next_twin(xmlroom);
	}
	purple_roomlist_set_in_progress(xaction->campfire->roomlist, FALSE);
}

void campfire_room_query(CampfireConn *campfire)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)(campfire_room_query_callback);
	xaction->response_cb_data = xaction;
	campfire_http_request(xaction, "/rooms.xml", "GET", NULL);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}


void campfire_room_update_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
                                PurpleInputCondition cond)
{
	campfire_room_check(xaction->campfire);
}

void campfire_room_update(CampfireConn *campfire, gint id, gchar *topic, gchar *room_name)
{	
	gchar *room_id = g_strdup_printf("%i", id);
	xmlnode *xmlroom = NULL, *xmltopic = NULL, *xmlname = NULL;
	CampfireSslTransaction *xaction = NULL;
	GString *uri = NULL;
	
	xmlroom = xmlnode_new("room");
	if(topic)
	{
		xmltopic = xmlnode_new_child(xmlroom, "topic");
		xmlnode_insert_data(xmltopic, topic, -1);
	}

	if(room_name)
	{
		xmlname = xmlnode_new_child(xmlroom, "name");
		xmlnode_insert_data(xmlname, room_name, -1);
	}
	
	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, ".xml");

	xaction = g_new0(CampfireSslTransaction, 1);
	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_room_update_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);

	purple_debug_info("campfire", "Sending message %s\n", xmlnode_to_str(xmlroom, NULL));
		
	campfire_http_request(xaction, uri->str, "PUT", xmlroom);
	g_string_free(uri, TRUE);
	g_free(room_id);
	xmlnode_free(xmlroom);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}

PurpleCmdRet campfire_parse_cmd(PurpleConversation *conv, const gchar *cmd,
										 gchar **args, gchar **error, void *data)
{
	PurpleConnection *gc = purple_conversation_get_gc(conv);
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
	GString *message = NULL;
	
	if (!gc)
		return PURPLE_CMD_RET_FAILED;
	
	purple_debug_info("campfire", "cmd %s: args[0]: %s\n", cmd, args[0]);

	if(g_strcmp0(cmd, CAMPFIRE_CMD_ME) == 0)
	{
		//send a message
		message = g_string_new("*");
		g_string_append(message, args[0]);
		g_string_append(message, "*");
		
		campfire_message_send(data, chat->id, message->str, CAMPFIRE_MESSAGE_TEXT);
	}
	else if(g_strcmp0(cmd, CAMPFIRE_CMD_PLAY) == 0)
	{
		//send a message
		campfire_message_send(data, chat->id, args[0], CAMPFIRE_MESSAGE_SOUND);
	}
	else if(g_strcmp0(cmd, CAMPFIRE_CMD_TOPIC) == 0)
	{
		//do a room request
		if(args[0])
			campfire_room_update(data, chat->id, args[0], NULL);
		else
			campfire_room_update(data, chat->id, "", NULL);
	}
	else if(g_strcmp0(cmd, CAMPFIRE_CMD_ROOM) == 0)
	{
		//do a room request
		campfire_room_update(data, chat->id, NULL, args[0]);
	}
	
	return PURPLE_CMD_RET_OK;
}

void campfire_fetch_first_messages(CampfireConn *campfire, gchar *room_id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	gint limit = purple_account_get_int(campfire->account, "limit", 10);
	GString *uri = NULL;
	
	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/recent.xml?limit=");
	g_string_append(uri, g_strdup_printf("%i", limit));
	

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_message_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);
	xaction->first_check = TRUE;

	campfire_http_request(xaction, uri->str, "GET", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}

void campfire_room_join_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireRoom *room = g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "joining room: %s with id: %s\n", room->name, room->id);
	serv_got_joined_chat(xaction->campfire->gc, g_ascii_strtoll(xaction->room_id, NULL, 10), room->name);
	campfire_fetch_first_messages(xaction->campfire, xaction->room_id);
}

void campfire_room_join(CampfireConn *campfire, gchar *id, gchar *name)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = g_string_new("/room/");
	CampfireRoom *room = NULL;

	g_string_append(uri, id);
	g_string_append(uri, "/join.xml");

	if(!campfire->rooms)
	{
		campfire->rooms = g_hash_table_new(g_str_hash, g_str_equal);
	}
	purple_debug_info("campfire", "add room to list %s ID: %s\n", name, id);
	room = g_new0(CampfireRoom, 1);
	room->name = g_strdup(name);
	room->id = g_strdup(id);
	g_hash_table_replace(campfire->rooms, id, room);

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_room_join_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(id);
	purple_debug_info("campfire", "ID: %s\n", xaction->room_id);

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}

void campfire_room_leave_callback(CampfireSslTransaction *xaction, PurpleSslConnection *gsc,
				    PurpleInputCondition cond)
{
	CampfireRoom *room = g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "leaving room: %s\n", room->name);
	serv_got_chat_left(xaction->campfire->gc, g_ascii_strtoll(xaction->room_id, NULL, 10));
	gboolean left = g_hash_table_remove(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "left room: %s\n", left ? "true" : "false");
}

void campfire_room_leave(CampfireConn *campfire, gint id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = NULL;

	xaction->campfire = campfire;
	xaction->response_cb = (PurpleSslInputFunction)campfire_room_leave_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup_printf("%d", id);

	uri = g_string_new("/room/");
	g_string_append(uri, xaction->room_id);
	g_string_append(uri, "/leave.xml");

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction, PURPLE_INPUT_READ|PURPLE_INPUT_WRITE);
}
