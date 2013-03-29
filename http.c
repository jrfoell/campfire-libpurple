
/*local includes*/
#include "http.h"
#include "message.h"

/*system includes*/
#include <errno.h>

/*purple includes*/
#include <debug.h>

static void
campfire_ssl_failure(G_GNUC_UNUSED PurpleSslConnection * gsc,
		     G_GNUC_UNUSED PurpleSslErrorType error,
		     G_GNUC_UNUSED gpointer data)
{
	purple_debug_info("campfire", "ssl connect failure\n");
}

void
campfire_http_request(CampfireSslTransaction * xaction, gchar * uri,
		      gchar * method, xmlnode * postxml)
{
	CampfireConn *conn = xaction->campfire;
	const char *api_token =
		purple_account_get_string(conn->account, "api_token", "");
	gchar *xmlstr = NULL, *len = NULL, *encoded = NULL;
	gsize auth_len;

	xaction->http_request = g_string_new(method);
	g_string_append(xaction->http_request, " ");
	g_string_append(xaction->http_request, uri);
	g_string_append(xaction->http_request, " HTTP/1.1\r\n");

	g_string_append(xaction->http_request,
			"Content-Type: application/xml\r\n");

	g_string_append(xaction->http_request, "Authorization: Basic ");
	auth_len = strlen(api_token);
	encoded = purple_base64_encode((const guchar *) api_token, auth_len);
	g_string_append(xaction->http_request, encoded);
	g_string_append(xaction->http_request, "\r\n");
	g_free(encoded);

	g_string_append(xaction->http_request, "Host: ");
	g_string_append(xaction->http_request, conn->hostname);
	g_string_append(xaction->http_request, "\r\n");

	g_string_append(xaction->http_request, "Accept: */*\r\n");

	if (postxml) {
		xmlstr = xmlnode_to_str(postxml, NULL);
		g_string_append(xaction->http_request, "Content-Length: ");
		/* len = g_strdup_printf("%lu", strlen(xmlstr)); */
		len = g_strdup_printf("%" G_GSIZE_FORMAT, strlen(xmlstr));
		g_string_append(xaction->http_request, len);
		g_free(len);
		g_string_append(xaction->http_request, "\r\n\r\n");
		g_string_append(xaction->http_request, xmlstr);
		g_string_append(xaction->http_request, "\r\n");
	}
	g_string_append(xaction->http_request, "\r\n");
	purple_debug_info("campfire", "Formatted request:\n%s\n",
			  xaction->http_request->str);
}

/* return 1 on error, return 0 on success */
static gint
campfire_get_http_status_from_header(gint * status, GString * header)
{
	gchar *str, *found_str, *extra_chars;
	gchar prefix[] = "\r\nStatus: ";
	gsize prefix_size = strlen(prefix);
	gint tmp;

	found_str = g_strstr_len(header->str, header->len, prefix);
	if (!found_str) {
		return 1;
	}
	found_str += prefix_size;	/* increment pointer to start of status string */
	str = g_malloc0(4);	/* status is 3-digits plus NULL */
	g_strlcpy(str, found_str, 4);
	tmp = (gint) g_ascii_strtoull(str, &extra_chars, 10);
	g_free(str);
	if (tmp == 0 && extra_chars == str) {
		*status = -1;
		return 1;
	}
	*status = tmp;
	return 0;
}

/* return 1 on error, return 0 on success */
static gint
campfire_get_content_length_from_header(gsize * cl, GString * header)
{
	gchar *str, *found_str, *extra_chars, *eol;
	gchar prefix[] = "\r\nContent-Length: ";
	gsize prefix_size = strlen(prefix);
	gsize rest_of_header_size;
	gsize cl_size;
	gsize tmp;

	found_str = g_strstr_len(header->str, header->len, prefix);
	if (!found_str) {
		return 1;
	}
	found_str += prefix_size;	/* increment pointer to start desired string */
	rest_of_header_size = header->len - (found_str - header->str);
	eol = g_strstr_len(found_str, rest_of_header_size, "\r\n");
	if (!eol) {
		return 1;
	}
	cl_size = eol - found_str;
	purple_debug_info("campfire", "content length str len = %"
			  G_GSIZE_FORMAT "\n", cl_size);
	str = g_malloc0(cl_size + 1);
	g_strlcpy(str, found_str, cl_size + 1);
	tmp = g_ascii_strtoll(str, &extra_chars, 10);
	g_free(str);
	if (tmp == 0 && extra_chars == str) {
		*cl = 0;
		return 1;
	}
	purple_debug_info("campfire", "content length: %" G_GSIZE_FORMAT "\n",
			  tmp);
	*cl = tmp;
	return 0;
}

static gboolean
ssl_input_consumed(GString * ssl_input)
{
	gboolean consumed = FALSE;
	if (ssl_input == NULL) {
		consumed = TRUE;
	} else if (ssl_input->len == 0) {
		consumed = TRUE;
	}
	return consumed;
}

static void
campfire_consume_http_header(CampfireHttpResponse * response,
			     GString * ssl_input)
{
	gchar blank_line[] = "\r\n\r\n";
	gchar *header_end;
	header_end = g_strstr_len(ssl_input->str, ssl_input->len, blank_line);
	gsize header_len;
	if (header_end) {
		header_end += strlen(blank_line);
		header_len = header_end - ssl_input->str;
	} else {
		header_len = ssl_input->len;
	}
	g_string_append_len(response->response, ssl_input->str, header_len);
	g_string_erase(ssl_input, 0, header_len);
}

static gboolean
campfire_http_header_received(CampfireHttpResponse * response)
{
	gboolean received = FALSE;
	gchar blank_line[] = "\r\n\r\n";
	gchar *header_end;
	header_end =
		g_strstr_len(response->response->str, response->response->len,
			     blank_line);
	if (header_end) {
		received = TRUE;
	}
	return received;
}

/* this function should be called just after the header (including  blank line
 * have been copied from ssl_input to the response string.  That way this
 * operation to save the header string becomes a simple string copy from
 * one GString to another.
 */
static void
campfire_process_http_header(CampfireHttpResponse * response)
{
	response->header = g_string_new("");
	g_string_append(response->header, response->response->str);
	campfire_get_http_status_from_header(&response->status,
					     response->header);
	campfire_get_content_length_from_header(&response->content_len,
						response->header);
}

static void
campfire_consume_http_content(CampfireHttpResponse * response,
			      GString * ssl_input)
{
	g_string_append_len(response->response, ssl_input->str, ssl_input->len);
	response->content_received_len += ssl_input->len;
	g_string_erase(ssl_input, 0, -1);
}

static gboolean
campfire_http_content_received(CampfireHttpResponse * response)
{
	return response->content_received_len >= response->content_len;
}

/* this function should be called just after the received bytes match the
 * content length prescribe in the http header.  To create the 'content'
 * GString we must copy the 'response' string starting immediately after
 * the blank line
 */
static void
campfire_process_http_content(CampfireHttpResponse * response)
{
	gsize content_start_index = response->header->len;
	response->content = g_string_new("");
	g_string_append(response->content,
			&response->response->str[content_start_index]);
}

static gint
campfire_http_response(PurpleSslConnection * gsc,
		       CampfireSslTransaction * xaction,
		       G_GNUC_UNUSED PurpleInputCondition cond,
		       G_GNUC_UNUSED xmlnode ** node)
{
	gchar buf[1024];
	GString *ssl_input;
	gint len, errsv = 0;
	gint status;
	CampfireHttpResponse *response = &xaction->http_response;

	if (response->rx_state == CAMPFIRE_HTTP_RX_DONE) {
		purple_debug_info("campfire", "somefin aint right.\n");
		return -1;
	}

	/**********************************************************************
	 * read input from file descriptor
	 *********************************************************************/
	ssl_input = g_string_new("");
	errno = 0;
	while ((len = purple_ssl_read(gsc, buf, sizeof(buf))) > 0) {
		purple_debug_info("campfire",
				  "read %d bytes from HTTP Response, errno: %i\n",
				  len, errno);
		ssl_input = g_string_append_len(ssl_input, buf, len);
	}
	errsv = errno;

	/**********************************************************************
	 * handle return value of ssl input read
	 *********************************************************************/
	if (len < 0 && errsv == EAGAIN) {
		if (ssl_input->len == 0) {
			purple_debug_info("campfire",
					  "TRY AGAIN (returning)\n");
			g_string_free(ssl_input, TRUE);
			return 0;
		} else {
			purple_debug_info("campfire", "EAGAIN (continuing)\n");
		}
	} else if (len == 0) {
		purple_debug_info("campfire", "SERVER CLOSED CONNECTION\n");
		if (ssl_input->len == 0) {
			g_string_free(ssl_input, TRUE);
			return -1;
		}
	} else {
		purple_debug_info("campfire", "LOST CONNECTION\n");
		purple_debug_info("campfire", "errno: %d\n", errsv);
		g_string_free(ssl_input, TRUE);
		return -1;
	}

	if (!response->response) {
		response->response = g_string_new("");
	}

	purple_debug_info("campfire", "ssl_input:\n%s", ssl_input->str);

	/**********************************************************************
	 * process input with a simple state machine
	 *********************************************************************/
	while (!ssl_input_consumed(ssl_input)) {
		switch (response->rx_state) {
		case CAMPFIRE_HTTP_RX_HEADER:
			purple_debug_info("campfire",
					  "CAMPFIRE_HTTP_RX_HEADER\n");
			campfire_consume_http_header(response, ssl_input);
			if (campfire_http_header_received(response)) {
				campfire_process_http_header(response);
				response->rx_state = CAMPFIRE_HTTP_RX_CONTENT;
			}
			break;
		case CAMPFIRE_HTTP_RX_CONTENT:
			purple_debug_info("campfire",
					  "CAMPFIRE_HTTP_RX_CONTENT\n");
			campfire_consume_http_content(response, ssl_input);
			if (campfire_http_content_received(response)) {
				campfire_process_http_content(response);
				response->rx_state = CAMPFIRE_HTTP_RX_DONE;
			}
			break;
		case CAMPFIRE_HTTP_RX_DONE:
			purple_debug_info("campfire",
					  "CAMPFIRE_HTTP_RX_DONE\n");
			g_string_erase(ssl_input, 0, -1);	/* consume input */
			break;
		default:
			g_string_erase(ssl_input, 0, -1);	/* consume input */
			break;
		}
	}

	/**********************************************************************
	 * return http status code: -1=error, 0=input_not_received
	 *********************************************************************/
	if (response->rx_state == CAMPFIRE_HTTP_RX_DONE) {
		status = response->status;
	} else {
		status = 0;
	}
	g_string_free(ssl_input, TRUE);
	return status;
}


void
campfire_message_free(gpointer data)
{
	CampfireMessage *msg = (CampfireMessage *)(data);
	if (msg != NULL) {
		if (msg->id != NULL) {
			g_free(msg->id);
		}
		if (msg->type != NULL) {
			g_free(msg->type);
		}
		if (msg->message != NULL) {
			g_free(msg->message);
		}
		if (msg->user_id != NULL) {
			g_free(msg->user_id);
		}
		g_free(msg);
	}
}

void
campfire_xaction_free(CampfireSslTransaction *xaction)
{
	if (xaction) {
		if (xaction->http_request) {
			g_string_free(xaction->http_request, TRUE);
		}
		if (xaction->http_response.response) {
			g_string_free(xaction->http_response.response,
				      TRUE);
		}
		if (xaction->http_response.header) {
			g_string_free(xaction->http_response.header,
				      TRUE);
		}
		if (xaction->http_response.content) {
			g_string_free(xaction->http_response.content,
				      TRUE);
		}
		if (xaction->xml_response) {
			xmlnode_free(xaction->xml_response);
		}
		if (xaction->room_id) {
			g_free(xaction->room_id);
		}
		g_list_free_full(xaction->messages, &campfire_message_free);
		xaction->campfire->num_xaction_free++; /* valgrind investigation */
		purple_debug_info("campfire", "xaction: %p, num_xaction_malloc:%d: num_xaction_free:%d\n",
		                  xaction,
		                  xaction->campfire->num_xaction_malloc,
		                  xaction->campfire->num_xaction_free);
		g_free(xaction);
	}

}

static void
campfire_ssl_handler(CampfireConn * campfire,
		     PurpleSslConnection * gsc, PurpleInputCondition cond)
{
	GList *first = g_list_first(campfire->queue);
	CampfireSslTransaction *xaction = NULL;
	gint status;
	gboolean close_ssl = FALSE;
	gboolean cleanup = TRUE;

	if (first) {
		xaction = first->data;
	} else {
		xaction = g_new0(CampfireSslTransaction, 1);
		campfire->num_xaction_malloc++; /* valgrind investigation */
		purple_debug_info("campfire", "%s: xaction: %p, campfire->num_xaction_malloc:%d\n",
		                  __FUNCTION__, xaction, campfire->num_xaction_malloc);
		xaction->campfire = campfire;
	}

	purple_debug_info("campfire", "%s: first: %p\n", __FUNCTION__, first);

	status = campfire_http_response(gsc, xaction, cond,
					&(xaction->xml_response));
	purple_debug_info("campfire", "http status: %d\n", status);

	if (status == 200 || status == 201) {
		xaction->xml_response =
			xmlnode_from_str(xaction->http_response.content->str,
					 -1);
		if (xaction && xaction->response_cb) {
			purple_debug_info("campfire",
			                  "calling response_cb (%p)\n",
			                  xaction->response_cb);
			xaction->response_cb(xaction, gsc, cond);
		}
		cleanup = TRUE;
	} else if (status == 0) {	/*received partial content */
		cleanup = FALSE;
	} else {		/*status < 0 or some other http status we don't expect */

		close_ssl = TRUE;
		cleanup = TRUE;
	}

	if (close_ssl && campfire->gsc) {
		purple_debug_info("campfire",
				  "closing ssl connection:%p (%p)\n", gsc,
				  campfire->gsc);
		campfire->gsc = NULL;
		purple_ssl_close(gsc);
		cleanup = TRUE;
	}

	if (cleanup) {
		campfire_xaction_free(xaction);

		if (first) {
			purple_debug_info("campfire",
					  "removing from queue: length: %d\n",
					  g_list_length(campfire->queue));
			campfire->queue =
				g_list_remove(campfire->queue, xaction);
			purple_debug_info("campfire",
					  "removed from queue: length: %d\n",
					  g_list_length(campfire->queue));
		}

		first = g_list_first(campfire->queue);
		if (first) {
			xaction = first->data;
			purple_debug_info("campfire",
					  "writing subsequent request on ssl connection\n");
			purple_ssl_write(gsc, xaction->http_request->str,
					 xaction->http_request->len);
		}
	}
}

/*
 * this prototype is needed because the next two functions call each other.
 */
static void campfire_ssl_connect(CampfireConn * campfire,
				 PurpleInputCondition cond,
				 gboolean from_connection_callback);

/* This is just an itermediate function:
 * It is called by lower level purple stuff when the ssl connection is
 * established.  It merely calls 'campfire_ssl_connect' with an extra argument
 * to denote that it's coming from the callback.
 */
static void
campfire_ssl_connect_cb(CampfireConn * campfire, PurpleInputCondition cond)
{
	campfire_ssl_connect(campfire, cond, TRUE);
}

static void
campfire_ssl_connect(CampfireConn * campfire,
		     G_GNUC_UNUSED PurpleInputCondition cond,
		     gboolean from_connection_callback)
{
	GList *first = NULL;
	CampfireSslTransaction *xaction = NULL;

	purple_debug_info("campfire", "%s\n", __FUNCTION__);
	if (!campfire) {
		return;
	} else {
		first = g_list_first(campfire->queue);
	}

	if (!first) {
		return;
	} else {
		xaction = first->data;
	}

	if (!xaction) {
		return;
	}

	if (!campfire->gsc) {
		purple_debug_info("campfire", "new ssl connection\n");
		campfire->gsc = purple_ssl_connect(campfire->account,
						   campfire->hostname,
						   443, (PurpleSslInputFunction)
						   (campfire_ssl_connect_cb),
						   campfire_ssl_failure,
						   campfire);
		purple_debug_info("campfire",
				  "new ssl connection kicked off.\n");
	} else {
		purple_debug_info("campfire", "previous ssl connection\n");
		/* we want to write our http request to the ssl connection
		 * WHENEVER this is called from the callback (meaning we've
		 * JUST NOW established the connection). OR when the first
		 * transaction is added to the queue on an OPEN ssl connection
		 */
		if (from_connection_callback
		    || g_list_length(campfire->queue) == 1) {
			/* campfire_ssl_handler is the ONLY input handler we
			 * EVER use So... if there is already an input handler
			 * present (inpa > 0), then we DON"T want to add another
			 * input handler.  Quite a few hours spent chasing bugs
			 * when multiple input handlers were added!
			 */
			if (campfire->gsc->inpa == 0) {
				purple_debug_info("campfire", "adding input\n");
				purple_ssl_input_add(campfire->gsc,
						     (PurpleSslInputFunction)
						     (campfire_ssl_handler),
						     campfire);
			}
			purple_debug_info("campfire",
					  "writing first request on ssl connection\n");
			purple_ssl_write(campfire->gsc,
					 xaction->http_request->str,
					 xaction->http_request->len);
		}
	}
	return;
}

void
campfire_queue_xaction(CampfireConn * campfire,
		       CampfireSslTransaction * xaction,
		       PurpleInputCondition cond)
{
	gboolean from_callback = FALSE;	/* this is not the ssl connection callback */
	purple_debug_info("campfire", "%s input condition: %i\n", __FUNCTION__,
			  cond);
	xaction->queued = TRUE;
	campfire->queue = g_list_append(campfire->queue, xaction);
	purple_debug_info("campfire", "queue length %d\n",
			  g_list_length(campfire->queue));
	campfire_ssl_connect(campfire, cond, from_callback);
}
