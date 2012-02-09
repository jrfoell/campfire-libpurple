#ifndef HTTP_H
#define HTTP_H

//local includes
#include "campfire.h"

//system includes
#include <glib/gi18n.h>

//purple includes
#include <xmlnode.h>
#include <plugin.h>

enum http_response_status {
	CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK,
	CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML,
	CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN,
	CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION,
	CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED,
	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT,
	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML,
};

typedef struct _CampfireSslTransaction {
	CampfireConn *campfire;
	GString *http_request;
	GString *http_response;
	PurpleSslInputFunction response_cb;
	gpointer response_cb_data;
	xmlnode *xml_response;
	gint content_len;
	//optional
	gchar *room_id;
	GList *messages;
	gboolean first_check;
	gboolean my_message;
} CampfireSslTransaction;

void campfire_http_request(CampfireSslTransaction *xaction, gchar *uri, gchar *method, xmlnode *postxml);
void campfire_queue_xaction(CampfireConn *campfire, CampfireSslTransaction *xaction, PurpleInputCondition cond);

#endif /* not HTTP_H */
