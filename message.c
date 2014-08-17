
/* local includes */
#include "campfire.h"
#include "message.h"
#include "http.h"

/* purple includes */
#include <debug.h>

/* internal function prototypes */
void
campfire_message_handler_callback(CampfireSslTransaction * xaction,
				       PurpleSslConnection * gsc,
				       PurpleInputCondition cond);

static CampfireMessage *
campfire_get_message(xmlnode * xmlmessage)
{
	xmlnode *xmlbody = NULL, *xmluser_id = NULL, *xmltime = NULL,
		*xmltype = NULL, *xmlid = NULL;
	gchar *body = NULL, *user_id = NULL, *msgtype = NULL, *msg_id = NULL;
	gchar *xmltime_data;
	CampfireMessage *msg = NULL;
	GTimeVal timeval;
	time_t mtime = 0;

	xmlbody = xmlnode_get_child(xmlmessage, "body");
	body = xmlnode_get_data(xmlbody); /* needs g_free */

	xmluser_id = xmlnode_get_child(xmlmessage, "user-id");
	user_id = xmlnode_get_data(xmluser_id); /* needs g_free */

	xmltime = xmlnode_get_child(xmlmessage, "created-at");
	xmltime_data = xmlnode_get_data(xmltime); /* needs g_free */
	if (g_time_val_from_iso8601(xmltime_data, &timeval)) {
		mtime = timeval.tv_sec;
	}
	g_free(xmltime_data);

	xmltype = xmlnode_get_child(xmlmessage, "type");
	msgtype = xmlnode_get_data(xmltype); /* needs g_free */

	xmlid = xmlnode_get_child(xmlmessage, "id");
	msg_id = xmlnode_get_data(xmlid); /* needs g_free */

	if (g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TIME) == 0) {
		purple_debug_info("campfire", "Skipping message of type: %s\n",
				  msgtype);
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
	    g_strcmp0(msgtype, CAMPFIRE_MESSAGE_TOPIC) == 0) {
		msg->message = body;
	} else {
		g_free(body);
	}
	return msg;
}

static void
message_g_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
	g_free(data);
}

static void
campfire_userlist_callback(CampfireSslTransaction * xaction,
			   G_GNUC_UNUSED PurpleSslConnection * gsc,
			   G_GNUC_UNUSED PurpleInputCondition cond)
{
	PurpleConversation *convo = NULL;
	PurpleConvChat *chat = NULL;
	xmlnode *xmlroomname = NULL, *xmltopic = NULL,
		*xmlusers =	NULL, *xmluser = NULL, *xmlname = NULL;
	gchar *room_name = NULL, *topic = NULL, *name = NULL;
	GList *users = NULL, *chatusers = NULL, *users_iter = NULL;
	PurpleConvChatBuddy *buddy = NULL;
	gboolean found;

	xmlroomname = xmlnode_get_child(xaction->xml_response, "name");
	room_name = xmlnode_get_data(xmlroomname); /* needs g_free */
	purple_debug_info("campfire", "locating room: %s\n", room_name);
	convo = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY,
						      room_name,
						      purple_connection_get_account
						      (xaction->campfire->gc));
	g_free(room_name);
	chat = PURPLE_CONV_CHAT(convo);
	xmltopic = xmlnode_get_child(xaction->xml_response, "topic");
	topic = xmlnode_get_data(xmltopic); /* needs g_free */
	purple_debug_info("campfire", "setting topic to %s\n", topic);
	purple_conv_chat_set_topic(chat, NULL, topic);
	g_free(topic);
	xmlusers = xmlnode_get_child(xaction->xml_response, "users");
	xmluser = xmlnode_get_child(xmlusers, "user");

	while (xmluser != NULL) {
		xmlname = xmlnode_get_child(xmluser, "name");
		name = xmlnode_get_data(xmlname); /* needs g_free */
		purple_debug_info("campfire", "user in room: %s\n", name);

		if (!purple_conv_chat_find_user(chat, name)) {
			purple_debug_info("campfire",
					  "adding user %s to room\n", name);
			purple_conv_chat_add_user(chat, name,
						  NULL, PURPLE_CBFLAGS_NONE,
						  TRUE);
		}
		users = g_list_prepend(users, name);
		xmluser = xmlnode_get_next_twin(xmluser);
	}

	purple_debug_info("campfire", "Getting all users in room\n");
	chatusers = purple_conv_chat_get_users(chat);
	purple_debug_info("campfire", "got all users in room %p\n", chatusers);

	if (users == NULL) {
		/**
		 * Can happen if you're the last user in the room, and you clicked "Leave" through
		 * the web interface
		 */
		purple_debug_info("campfire", "removing all users from room\n");
		purple_conv_chat_remove_users(chat,
					      chatusers, NULL);
	} else if (chatusers != NULL) {
		GList *remove = NULL;
		purple_debug_info("campfire", "iterating chat users\n");
		for (; chatusers != NULL; chatusers = chatusers->next) {
			buddy = chatusers->data;
			found = FALSE;
			purple_debug_info("campfire",
					  "checking to see if user %s has left\n",
					  buddy->name);
			for (users_iter = users; users_iter; users_iter = users_iter->next) {
				if (g_strcmp0(users_iter->data, buddy->name) == 0) {
					purple_debug_info("campfire",
							  "user %s is still here\n",
							  buddy->name);
					found = TRUE;
					break;
				}
			}

			if (!found) {
				purple_debug_info("campfire",
						  "removing user %s that has left\n",
						  buddy->name);
				remove = g_list_prepend(remove,
							g_strdup(buddy->name));
			}
		}
		if (remove) {
			purple_conv_chat_remove_users(chat, remove, "left");
			g_list_foreach(remove, message_g_free, NULL);
			g_list_free(remove);
		}
		g_list_foreach(users, message_g_free, NULL);
		g_list_free(users);
	}
}

static CampfireSslTransaction *
campfire_new_xaction_copy(CampfireSslTransaction * original)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GList *msgs = NULL;
	CampfireMessage *msg = NULL;

	original->campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, original->campfire->num_xaction_malloc);
	xaction->campfire = original->campfire;
	xaction->response_cb_data = xaction;

	/*xaction->messages = original->messages;*/ /* can't do this safely */
	msgs = original->messages;
	for (; msgs != NULL; msgs = msgs->next) {
		CampfireMessage *orig_msg = msgs->data;
		msg = g_new0(CampfireMessage, 1);

		msg->id      = g_strdup(orig_msg->id);
		msg->type    = g_strdup(orig_msg->type);
		msg->message = g_strdup(orig_msg->message);
		msg->time    = orig_msg->time;
		msg->user_id = g_strdup(orig_msg->user_id);
		xaction->messages = g_list_append(xaction->messages, msg);
	}

	xaction->first_check = original->first_check;
	/* make a copy of the room id (if it's set) b/c it might
	 * get free'd by someone if we just use the pointer
	 */
	if (original->room_id)
		xaction->room_id = g_strdup(original->room_id);

	return xaction;
}

static void
campfire_message_callback(CampfireSslTransaction * xaction,
			  G_GNUC_UNUSED PurpleSslConnection * gsc,
			  G_GNUC_UNUSED PurpleInputCondition cond)
{
	xmlnode *xmlmessage = NULL;
	GList *msgs = NULL;
	CampfireMessage *msg = NULL;
	CampfireSslTransaction *xaction2 = NULL;

	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	xmlmessage = xmlnode_get_child(xaction->xml_response, "message");

	while (xmlmessage != NULL) {
		msg = campfire_get_message(xmlmessage);
		if (msg)
			msgs = g_list_append(msgs, msg);

		xmlmessage = xmlnode_get_next_twin(xmlmessage);
	}

	xaction2 = campfire_new_xaction_copy(xaction);
	xaction2->response_cb =
		(PurpleSslInputFunction) campfire_message_handler_callback;
	xaction2->response_cb_data = xaction2;
	xaction2->messages = msgs;

	campfire_message_handler_callback(xaction2, xaction2->campfire->gsc,
					  PURPLE_INPUT_READ);
}

static gboolean
campfire_room_check(CampfireConn * campfire)
{
	GList *rooms = g_hash_table_get_values(campfire->rooms);
	CampfireRoom *room = NULL;
	CampfireSslTransaction *xaction = NULL, *xaction2 = NULL;
	GString *uri = NULL;

	/* cancel the timer if we've left all rooms */
	if (!rooms) {
		purple_debug_info("campfire",
				  "not in any rooms, removing timer\n");
		campfire->message_timer = 0;
		return FALSE;
	}

	for (; rooms != NULL; rooms = rooms->next) {
		room = rooms->data;

		/* first check the room users */
		xaction = g_new0(CampfireSslTransaction, 1);
		campfire->num_xaction_malloc++; /* valgrind investigation */
		purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
		                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
		xaction->campfire = campfire;
		xaction->response_cb =
			(PurpleSslInputFunction) campfire_userlist_callback;
		xaction->response_cb_data = xaction;

		purple_debug_info("campfire",
				  "checking for users in room: %s\n", room->id);

		uri = g_string_new("/room/");
		g_string_append(uri, room->id);
		g_string_append(uri, ".xml");

		campfire_http_request(xaction, uri->str, "GET", NULL);
		campfire_queue_xaction(campfire, xaction,
				       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
		g_string_free(uri, TRUE);

		/* then get recent messages
		 * (only if there is nothing in the queue) */
		if (room->last_message_id) {

			xaction2 = campfire_new_xaction_copy(xaction);
			xaction2->response_cb =
				(PurpleSslInputFunction)
				campfire_message_callback;
			xaction2->room_id = g_strdup(room->id);

			uri = g_string_new("/room/");
			g_string_append(uri, room->id);
			g_string_append(uri, "/recent.xml?since_message_id=");
			g_string_append(uri, room->last_message_id);

			purple_debug_info("campfire",
					  "getting latest messages: %s\n",
					  uri->str);

			campfire_http_request(xaction2, uri->str, "GET", NULL);
			g_string_free(uri, TRUE);
			campfire_queue_xaction(campfire, xaction2,
					       PURPLE_INPUT_READ |
					       PURPLE_INPUT_WRITE);
		}
	}

	if (!campfire->message_timer) {
		/* call this function again periodically to check for new users */
		campfire->message_timer =
			purple_timeout_add_seconds(3,
						   (GSourceFunc)
						   campfire_room_check,
						   campfire);
	}

	g_list_free(rooms);
	return TRUE;
}

static void
campfire_request_user(CampfireSslTransaction * xaction, CampfireMessage * msg)
{
	CampfireSslTransaction *xaction2 = campfire_new_xaction_copy(xaction);
	GString *uri = g_string_new("/users/");

	g_string_append(uri, msg->user_id);
	g_string_append(uri, ".xml");

	xaction2->response_cb =
		(PurpleSslInputFunction) campfire_message_handler_callback;

	campfire_http_request(xaction2, uri->str, "GET", NULL);
	campfire_queue_xaction(xaction->campfire, xaction2, PURPLE_INPUT_READ);
	g_string_free(uri, TRUE);
}

static void
campfire_request_upload(CampfireSslTransaction * xaction, CampfireMessage * msg)
{
	CampfireSslTransaction *xaction2 = campfire_new_xaction_copy(xaction);
	GString *uri = g_string_new("/room/");

	purple_debug_info("campfire", "Going to fetch upload\n");

	g_string_append(uri, xaction->room_id);
	g_string_append(uri, "/messages/");
	g_string_append(uri, msg->id);
	g_string_append(uri, "/upload.xml");

	xaction2->response_cb =
		(PurpleSslInputFunction) campfire_message_handler_callback;

	campfire_http_request(xaction2, uri->str, "GET", NULL);
	campfire_queue_xaction(xaction->campfire, xaction2, PURPLE_INPUT_READ);
}

static void
campfire_print_message(CampfireConn *campfire, CampfireRoom * room, CampfireMessage * msg,
		       gchar * user_name, gchar * upload_url)
{
	PurpleConversation *convo = purple_find_conversation_with_account
			(PURPLE_CONV_TYPE_ANY, room->name,
			 purple_connection_get_account(campfire->gc));
	GString *message = NULL;

	if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TEXT) == 0 ||
	    g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TWEET) == 0 ||
	    g_strcmp0(msg->type, CAMPFIRE_MESSAGE_PASTE) == 0) {
		purple_debug_info("campfire",
				  "Writing chat message \"%s\" to %p from name %s\n",
				  msg->message, convo,
				  user_name);
		const gchar *nick = purple_account_get_string(campfire->account, "nicks", "LOL");
		int flag = PURPLE_MESSAGE_RECV;
		gchar s[256];
		strcpy(s, nick);
		gchar* token = strtok(s, " ");
		while(token) {
			if (purple_utf8_has_word(msg->message, token)) {
				flag |= PURPLE_MESSAGE_NICK;
				break;
			}
			token = strtok(NULL, " ");
		}

		purple_conversation_write(convo, user_name,
					  replace(replace(msg->message, "<", "&lt;"), ">", "&gt;"),
					  flag,
					  msg->time);
	} else {
		message = g_string_new(user_name);
		if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_ENTER) == 0) {
			g_string_append(message,
					" has entered the room.");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_LEAVE) == 0) {
			g_string_append(message,
					" has left the room.");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_KICK) == 0) {
			g_string_append(message, " kicked.");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_GUESTALLOW) == 0) {
			g_string_append(message,
					" turned on guest access.");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_GUESTDENY) == 0) {
			g_string_append(message,
					" turned off guest access.");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_TOPIC) == 0) {
			g_string_append(message,
					" changed the room's topic to \"");
			g_string_append(message, msg->message);
			g_string_append(message, "\"");
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_UPLOAD) == 0
			   && upload_url) {
			g_string_append(message, " uploaded ");
			g_string_append(message, upload_url);
		} else if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_SOUND) == 0) {
			g_string_append(message,
					" sounded off https://");
			g_string_append(message,
					campfire->hostname);
			g_string_append(message, "/sounds/");
			g_string_append(message, msg->message);
			g_string_append(message, ".mp3");
		}
		purple_conversation_write(convo, "",
					  message->str,
					  PURPLE_MESSAGE_SYSTEM,
					  msg->time);
		g_string_free(message, TRUE);
	}
}

static void
campfire_message_handler(CampfireSslTransaction * xaction,
			 CampfireMessage * msg, gchar * upload_url)
{

	CampfireConn *campfire = NULL;
	gchar *user_name = NULL;
	gchar *msg_id = NULL;
	gboolean print = TRUE;
	CampfireRoom *room = NULL;
	GList *tmp_buf = NULL;


	purple_debug_info("campfire", "xaction: (%p)\n", xaction);
	campfire = xaction->campfire;
	purple_debug_info("campfire", "msg: (%p)\n", msg);
	purple_debug_info("campfire", "user_id: %s\n", msg->user_id);
	user_name = g_hash_table_lookup(campfire->users, msg->user_id);
	purple_debug_info("campfire", "Looked up user_id: %s, got %s\n",
			  msg->user_id, user_name);

	if (!user_name) {
		campfire_request_user(xaction, msg);
		/* justin, this was your idea.  I see
		 * the need for it now.
		 */
		if (xaction->queued == FALSE) {
			campfire_xaction_free(xaction);
		}
	} else {
		purple_debug_info("campfire", "looking for room %s\n",
				  xaction->room_id);
		room = g_hash_table_lookup(campfire->rooms, xaction->room_id);
		purple_debug_info("campfire", "got room %p ID: %s name: %s last message id: %s\n", room, room->id, room->name, room->last_message_id);

		purple_debug_info("campfire",
				  "Writing message ID \"%s\" type \"%s\" from name \"%s\"\n",
				  msg->id, msg->type, user_name);

		/*
		 * In order to not print out your own messages twice, we'll
		 * keep a buffer of printed messages and check against that.
		 */
		purple_debug_info("campfire",
				  "Checking to see if message ID \"%s\" has been written\n",
				  msg->id);

		tmp_buf = g_list_first(room->message_id_buffer);

		for (; tmp_buf != NULL;
			 tmp_buf = tmp_buf->next) {
			msg_id = tmp_buf->data;
			purple_debug_info("campfire",
					  "Looking at message ID %s from list\n",
					  msg_id);

			if (g_strcmp0(msg_id, msg->id) == 0) {
				purple_debug_info("campfire",
						  "Won't write message \"%s\" already written\n",
						  msg_id);
				print = FALSE;
				break;
			}
		}

		if (print) {
			if (g_strcmp0(msg->type, CAMPFIRE_MESSAGE_UPLOAD) == 0
			    && !upload_url) {
				campfire_request_upload(xaction, msg);
				/* return here so we can print this message out
				 * again once we've retrieved the upload info
				 */
				return;
			} else {
				campfire_print_message(campfire, room, msg, user_name, upload_url);
			}
			/* remember the last message we've written */
			room->last_message_id = g_strdup(msg->id);
			purple_debug_info("campfire", "Adding message ID %s to buffer\n", msg->id);
			room->message_id_buffer = g_list_append(room->message_id_buffer, g_strdup(msg->id));
		}

		/* only keep 10 messages in the buffer */
		while (g_list_length(room->message_id_buffer) > 10) {
			purple_debug_info("campfire", "Removing message ID from buffer\n");
			room->message_id_buffer = g_list_delete_link(
				room->message_id_buffer,
				g_list_first(room->message_id_buffer));
		}
		campfire_message_free(msg, NULL);
		xaction->messages = g_list_remove(xaction->messages, msg);
		xaction->xml_response = NULL;
		/* recurse */
		campfire_message_handler_callback(xaction, campfire->gsc,
		                                  PURPLE_INPUT_READ |
		                                  PURPLE_INPUT_WRITE);
	}

}

void
campfire_message_handler_callback(CampfireSslTransaction * xaction,
				  G_GNUC_UNUSED PurpleSslConnection * gsc,
				  G_GNUC_UNUSED PurpleInputCondition cond)
{
	CampfireConn *campfire = xaction->campfire;
	GList *first = NULL;
	xmlnode *xmlusername = NULL, *xmluserid = NULL, *xmlurl = NULL;
	gchar *user_id = NULL, *username = NULL, *upload_url = NULL;

	purple_debug_info("campfire", "%s first_check:%s\n",
			  __FUNCTION__,
			  xaction->first_check ? "true" : "false");

	/* initialize user list */
	if (!campfire->users) {
		campfire->users = g_hash_table_new(g_str_hash, g_str_equal);
	}

	/* handle possible response(s)
	 * (only if used as a callback)
	 */
	if (xaction->xml_response) {
		char *xml_debug = xmlnode_to_str(xaction->xml_response, NULL);
		purple_debug_info("campfire", "got xml %s\n",
				  xml_debug);
		g_free(xml_debug);
		xmlurl = xmlnode_get_child(xaction->xml_response, "full-url");
		if (xmlurl) {
			/* it's an upload response */
			upload_url = xmlnode_get_data(xmlurl);
			purple_debug_info("campfire", "got upload URL %s\n",
					  upload_url);
		} else {
			/* it's a user response */
			xmlusername =
				xmlnode_get_child(xaction->xml_response,
						  "name");
			xmluserid =
				xmlnode_get_child(xaction->xml_response, "id");

			user_id = xmlnode_get_data(xmluserid);
			username = xmlnode_get_data(xmlusername);
			purple_debug_info("campfire",
					  "adding username %s ID %s\n",
					  username, user_id);
			g_hash_table_replace(campfire->users, user_id,
					     username);
		}
	}

	purple_debug_info("campfire",
			  "xaction->messages: (%p)\n",
			  xaction->messages);
	first = g_list_first(xaction->messages);
	purple_debug_info("campfire",
			  "first message: (%p)\n",
			  first);
	/* Do this while there are messages remaining in the GList */
	if (first) {
		/* process the next message */
		campfire_message_handler(xaction, first->data, upload_url);

	/* Do this after all messages have been processed */
	} else {
		purple_debug_info("campfire", "no more messages to process\n");
		purple_debug_info("campfire", "xaction: %p, xaction->queued: %d\n",
		                  xaction, xaction->queued);

		/* print the user as "in the room" after the previous messages
		 * have been printed (only if this is the first message check)
		 */
		if (xaction->first_check) {
			campfire_room_check(campfire);
		}
		/* 'un-queued' cleanup:
		 * NOTE: any transaction that is 'queued' using
		 * 'campfire_queue_xaction()' will be cleaned up in
		 * 'campfire_ssl_handler()'.  However, there is a transaction
		 * allocated in
		 * 'campfire_message_send_callback()'
		 * AND
		 * 'campfire_message_callback()'
		 * that are used but never 'queued', therefore it must be
		 * cleaned up here.
		 */
		if (xaction->queued == FALSE) {
			campfire_xaction_free(xaction);
		}
	}
}

static void
campfire_message_send_callback(CampfireSslTransaction * xaction,
			       G_GNUC_UNUSED PurpleSslConnection * gsc,
			       G_GNUC_UNUSED PurpleInputCondition cond)
{
	CampfireMessage *msg = campfire_get_message(xaction->xml_response);
	GList *msgs = NULL;
	CampfireSslTransaction *xaction2 = NULL;

	/* pretty much the same as campfire_message_callback
	 * but this one only processes one message
	 */
	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	msgs = g_list_append(msgs, msg);

	xaction2 = campfire_new_xaction_copy(xaction);
	xaction2->response_cb =
		(PurpleSslInputFunction) campfire_message_handler_callback;
	xaction2->messages = msgs;

	campfire_message_handler_callback(xaction2, xaction->campfire->gsc,
					  PURPLE_INPUT_READ);
}

void
campfire_message_send(CampfireConn * campfire, int id, const char *message,
		      char *msg_type)
{
	gchar *room_id = g_strdup_printf("%i", id);
	xmlnode *xmlmessage = NULL, *xmlchild = NULL;
	gchar *debug_str;
	gchar *unescaped;
	CampfireSslTransaction *xaction = NULL;
	GString *uri = NULL;

	if (!msg_type)
		msg_type = CAMPFIRE_MESSAGE_TEXT;

	xmlmessage = xmlnode_new("message");
	xmlnode_set_attrib(xmlmessage, "type", msg_type);
	xmlchild = xmlnode_new_child(xmlmessage, "body");
	unescaped = purple_unescape_html(message);
	xmlnode_insert_data(xmlchild, unescaped, -1);
	g_free(unescaped);

	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/speak.xml");

	xaction = g_new0(CampfireSslTransaction, 1);
	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) campfire_message_send_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);

	debug_str = xmlnode_to_str(xmlmessage, NULL);
	purple_debug_info("campfire", "Sending message %s\n",
			  debug_str);
	g_free(debug_str);

	campfire_http_request(xaction, uri->str, "POST", xmlmessage);
	g_string_free(uri, TRUE);
	g_free(room_id);
	xmlnode_free(xmlmessage);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}

static void
campfire_room_query_callback(CampfireSslTransaction * xaction,
			     G_GNUC_UNUSED PurpleSslConnection * gsc,
			     G_GNUC_UNUSED PurpleInputCondition cond)
{
	xmlnode *xmlroom = NULL, *xmlname = NULL, *xmltopic = NULL, *xmlid =
		NULL;
	gchar *name = NULL, *topic = NULL, *id = NULL;
	PurpleRoomlistRoom *room = NULL;

	purple_debug_info("campfire", "processing xml...\n");
	xmlroom = xmlnode_get_child(xaction->xml_response, "room");
	while (xmlroom != NULL) {
		xmlname = xmlnode_get_child(xmlroom, "name");
		name = xmlnode_get_data(xmlname); /* needs g_free */
		xmltopic = xmlnode_get_child(xmlroom, "topic");
		topic = xmlnode_get_data(xmltopic); /* needs g_free */
		xmlid = xmlnode_get_child(xmlroom, "id");
		id = xmlnode_get_data(xmlid); /* needs g_free */

		room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM,
						name, NULL);
		purple_roomlist_room_add_field(xaction->campfire->roomlist,
					       room, topic);
		purple_roomlist_room_add_field(xaction->campfire->roomlist,
					       room, id);
		purple_roomlist_room_add(xaction->campfire->roomlist, room);
		xmlroom = xmlnode_get_next_twin(xmlroom);
		g_free(name);
		g_free(topic);
		g_free(id);
	}
	purple_roomlist_set_in_progress(xaction->campfire->roomlist, FALSE);
	if (xaction->campfire->needs_join) {
		campfire_join_chat_after_room_query(xaction->campfire, xaction->campfire->desired_room);
	}
}

void
campfire_room_query(CampfireConn * campfire)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);

	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) (campfire_room_query_callback);
	xaction->response_cb_data = xaction;
	campfire_http_request(xaction, "/rooms.xml", "GET", NULL);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}


static void
campfire_room_update_callback(CampfireSslTransaction * xaction,
			      G_GNUC_UNUSED PurpleSslConnection * gsc,
			      G_GNUC_UNUSED PurpleInputCondition cond)
{
	campfire_room_check(xaction->campfire);
}

static void
campfire_room_update(CampfireConn * campfire, gint id, gchar * topic,
		     gchar * room_name)
{
	gchar *room_id = g_strdup_printf("%i", id);
	xmlnode *xmlroom = NULL, *xmltopic = NULL, *xmlname = NULL;
	CampfireSslTransaction *xaction = NULL;
	GString *uri = NULL;

	xmlroom = xmlnode_new("room");
	if (topic) {
		xmltopic = xmlnode_new_child(xmlroom, "topic");
		xmlnode_insert_data(xmltopic, topic, -1);
	}

	if (room_name) {
		xmlname = xmlnode_new_child(xmlroom, "name");
		xmlnode_insert_data(xmlname, room_name, -1);
	}

	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, ".xml");

	xaction = g_new0(CampfireSslTransaction, 1);
	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) campfire_room_update_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);

	purple_debug_info("campfire", "Sending message %s\n",
			  xmlnode_to_str(xmlroom, NULL));

	campfire_http_request(xaction, uri->str, "PUT", xmlroom);
	g_string_free(uri, TRUE);
	g_free(room_id);
	xmlnode_free(xmlroom);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}

PurpleCmdRet
campfire_parse_cmd(PurpleConversation * conv, const gchar * cmd,
		   gchar ** args, G_GNUC_UNUSED gchar ** error, void *data)
{
	PurpleConnection *gc = purple_conversation_get_gc(conv);
	PurpleConvChat *chat = PURPLE_CONV_CHAT(conv);
	GString *message = NULL;

	if (!gc)
		return PURPLE_CMD_RET_FAILED;

	purple_debug_info("campfire", "cmd %s: args[0]: %s\n", cmd, args[0]);

	if (g_strcmp0(cmd, CAMPFIRE_CMD_ME) == 0) {
		/* send a message */
		message = g_string_new("*");
		g_string_append(message, args[0]);
		g_string_append(message, "*");

		campfire_message_send(data, chat->id, message->str,
				      CAMPFIRE_MESSAGE_TEXT);
	} else if (g_strcmp0(cmd, CAMPFIRE_CMD_PLAY) == 0) {
		/* send a message */
		campfire_message_send(data, chat->id, args[0],
				      CAMPFIRE_MESSAGE_SOUND);
	} else if (g_strcmp0(cmd, CAMPFIRE_CMD_TOPIC) == 0) {
		/* do a room request */
		if (args[0])
			campfire_room_update(data, chat->id, args[0], NULL);
		else
			campfire_room_update(data, chat->id, "", NULL);
	} else if (g_strcmp0(cmd, CAMPFIRE_CMD_ROOM) == 0) {
		/* do a room request */
		campfire_room_update(data, chat->id, NULL, args[0]);
	}

	return PURPLE_CMD_RET_OK;
}

static void
campfire_fetch_first_messages(CampfireConn * campfire, gchar * room_id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	gint limit = purple_account_get_int(campfire->account, "limit", 10);
	gchar *limit_str;
	GString *uri = NULL;

	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	purple_debug_info("campfire", "%s\n", __FUNCTION__);

	uri = g_string_new("/room/");
	g_string_append(uri, room_id);
	g_string_append(uri, "/recent.xml?limit=");
	limit_str = g_strdup_printf("%i", limit);
	g_string_append(uri, limit_str);
	g_free(limit_str);


	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) campfire_message_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(room_id);
	xaction->first_check = TRUE;

	campfire_http_request(xaction, uri->str, "GET", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}

static gboolean
hide_buddy_join_cb(G_GNUC_UNUSED PurpleConversation *conv,
		   G_GNUC_UNUSED const char *name,
		   G_GNUC_UNUSED PurpleConvChatBuddyFlags flags,
		   G_GNUC_UNUSED void *data)
{
	return TRUE;
}

static gboolean
hide_buddy_leave_cb(G_GNUC_UNUSED PurpleConversation *conv,
		    G_GNUC_UNUSED const char *name,
		    G_GNUC_UNUSED const char *reason,
		    G_GNUC_UNUSED void *data)
{
	return TRUE;
}

static void
campfire_room_join_callback(CampfireSslTransaction * xaction,
			    G_GNUC_UNUSED PurpleSslConnection * gsc,
			    G_GNUC_UNUSED PurpleInputCondition cond)
{
	void *conv_handle = purple_conversations_get_handle();
	CampfireRoom *room =
		g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);

	purple_debug_info("campfire", "joining room: %s with id: %s\n",
			  room->name, room->id);
	serv_got_joined_chat(xaction->campfire->gc,
			     g_ascii_strtoll(xaction->room_id, NULL, 10),
			     room->name);

	/* hide pidgin's join/part messages (we'll do them ourselves) */
	purple_signal_connect(conv_handle, "chat-buddy-joining", xaction->campfire,
						  PURPLE_CALLBACK(hide_buddy_join_cb), NULL);
	purple_signal_connect(conv_handle, "chat-buddy-leaving", xaction->campfire,
						  PURPLE_CALLBACK(hide_buddy_leave_cb), NULL);

	campfire_fetch_first_messages(xaction->campfire, xaction->room_id);
}

void
campfire_room_join(CampfireConn * campfire, gchar * id, gchar * name)
{
	CampfireRoom *room = NULL;
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri;

	if (!campfire->rooms) {
		campfire->rooms = g_hash_table_new(g_str_hash, g_str_equal);
	} else {
		room = g_hash_table_lookup(campfire->rooms, id);
		if ( room ) {
			//already joined
			purple_debug_info("campfire", "already in room: %s with id: %s\n", name, id);
			purple_notify_message(campfire->gc, PURPLE_NOTIFY_MSG_ERROR,
		                      "campfire error",
		                      "already in this room.",
		                      NULL,
		                      NULL,
		                      NULL);
			return;
		}
	}

	uri = g_string_new("/room/");

	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	g_string_append(uri, id);
	g_string_append(uri, "/join.xml");

	purple_debug_info("campfire", "add room to list %s ID: %s\n", name, id);
	room = g_new0(CampfireRoom, 1);
	room->name = g_strdup(name);
	room->id = g_strdup(id);
	room->last_message_id = NULL;
	g_hash_table_replace(campfire->rooms, id, room);

	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) campfire_room_join_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup(id);
	purple_debug_info("campfire", "ID: %s\n", xaction->room_id);

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}

static void
campfire_room_leave_callback(CampfireSslTransaction * xaction,
			     G_GNUC_UNUSED PurpleSslConnection * gsc,
			     G_GNUC_UNUSED PurpleInputCondition cond)
{
	gboolean left;

	CampfireRoom *room =
		g_hash_table_lookup(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "leaving room: %s\n", room->name);
	serv_got_chat_left(xaction->campfire->gc,
			   g_ascii_strtoll(xaction->room_id, NULL, 10));
	left = g_hash_table_remove(xaction->campfire->rooms, xaction->room_id);
	purple_debug_info("campfire", "left room: %s\n",
			  left ? "true" : "false");
}

void
campfire_room_leave(CampfireConn * campfire, gint id)
{
	CampfireSslTransaction *xaction = g_new0(CampfireSslTransaction, 1);
	GString *uri = NULL;

	campfire->num_xaction_malloc++; /* valgrind investigation */
	purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
	                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
	xaction->campfire = campfire;
	xaction->response_cb =
		(PurpleSslInputFunction) campfire_room_leave_callback;
	xaction->response_cb_data = xaction;
	xaction->room_id = g_strdup_printf("%d", id);

	uri = g_string_new("/room/");
	g_string_append(uri, xaction->room_id);
	g_string_append(uri, "/leave.xml");

	campfire_http_request(xaction, uri->str, "POST", NULL);
	g_string_free(uri, TRUE);
	campfire_queue_xaction(campfire, xaction,
			       PURPLE_INPUT_READ | PURPLE_INPUT_WRITE);
}


char *replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}
