#ifndef HTTP_H
#define HTTP_H

/*local includes*/
#include "campfire.h"

/*system includes*/
#include <glib/gi18n.h>

/*purple includes*/
#include <xmlnode.h>
#include <plugin.h>

/* these are not used anymore
 * enum http_response_status {
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_MORE_CONTENT_NEEDED,
 * 	CAMPFIRE_HTTP_RESPONSE_STATUS_FAIL,
 * };
 */

typedef enum campfire_http_rx_state
{
	CAMPFIRE_HTTP_RX_HEADER,
	CAMPFIRE_HTTP_RX_CONTENT,
	CAMPFIRE_HTTP_RX_DONE,
} CampfireHttpRxState;

typedef struct _CampfireHttpResponse
{
	GString *response;
	GString *header;
	GString *content;
	gsize content_len;
	gsize content_received_len;
	gint status;
	CampfireHttpRxState rx_state;
} CampfireHttpResponse;

typedef struct _CampfireSslTransaction
{
	CampfireConn *campfire;
	GString *http_request;
	CampfireHttpResponse http_response;
	PurpleSslInputFunction response_cb;
	gpointer response_cb_data;
	xmlnode *xml_response;
	//optional
	gchar *room_id;
	GList *messages;
	gboolean first_check;
	gboolean my_message;
} CampfireSslTransaction;

void campfire_http_request(CampfireSslTransaction * xaction, gchar * uri,
			   gchar * method, xmlnode * postxml);
void campfire_queue_xaction(CampfireConn * campfire,
			    CampfireSslTransaction * xaction,
			    PurpleInputCondition cond);

#endif /* not HTTP_H */
