//local includes
#include "http.h"

//system includes
#include <errno.h>

//purple includes
#include <debug.h>

void campfire_ssl_failure(PurpleSslConnection *gsc, PurpleSslErrorType error, gpointer data)
{
	purple_debug_info("campfire", "ssl connect failure\n");
}

void campfire_http_request(CampfireSslTransaction *xaction, gchar *uri, gchar *method, xmlnode *postxml)
{
	CampfireConn *conn = xaction->campfire;
	const char *api_token = purple_account_get_string(conn->account, "api_token", "");
	gchar *xmlstr = NULL, *len = NULL, *encoded = NULL;
	gsize auth_len;

	xaction->http_request = g_string_new(method);
	g_string_append(xaction->http_request, " ");
	g_string_append(xaction->http_request, uri);
	g_string_append(xaction->http_request, " HTTP/1.1\r\n");

	g_string_append(xaction->http_request, "Content-Type: application/xml\r\n");
	
	g_string_append(xaction->http_request, "Authorization: Basic ");
	auth_len = strlen(api_token);
	encoded = purple_base64_encode((const guchar *)api_token, auth_len);
	g_string_append(xaction->http_request, encoded);
	g_string_append(xaction->http_request, "\r\n");	
	g_free(encoded);
	
	g_string_append(xaction->http_request, "Host: ");
	g_string_append(xaction->http_request, conn->hostname);
	g_string_append(xaction->http_request, "\r\n");

	g_string_append(xaction->http_request, "Accept: */*\r\n");

	if(postxml)
	{
		xmlstr = xmlnode_to_str(postxml, NULL);
		g_string_append(xaction->http_request, "Content-Length: ");
		len = g_strdup_printf("%lu", strlen(xmlstr));
		g_string_append(xaction->http_request, len);
		g_string_append(xaction->http_request, "\r\n\r\n");
		g_string_append(xaction->http_request, xmlstr);		
		g_string_append(xaction->http_request, "\r\n");
	}
	g_string_append(xaction->http_request, "\r\n");			
	purple_debug_info("campfire", "Formatted request:\n %s\n", xaction->http_request->str);
}

/* return 1 on error, return 0 on success */
static gint campfire_get_http_status_from_response(gint *status, GString *response)
{
	gchar *str, *found_str, *extra_chars;
	gchar prefix[] = "\r\nStatus: ";
	gsize prefix_size = sizeof(prefix) - 1; /* this sizeof() includes the NULL */
	gint tmp;

	found_str = g_strstr_len(response->str, response->len, prefix);
	if (!found_str) {
		return 1;
	}
	found_str += prefix_size; /* increment pointer to start of status string */
	str = g_malloc0(4); /* status is 3-digits plus NULL */
	g_strlcpy(str, found_str, 4);
	tmp = (gint)g_ascii_strtoull(str, &extra_chars, 10);
	g_free(str);
	if (tmp == 0  && extra_chars == str) {
		return 1;
	}
	*status = tmp;
	return 0;
}

/* return 1 on error, return 0 on success */
static gint campfire_get_content_length_from_response(gsize *cl, GString *response)
{
	gchar *str, *found_str, *extra_chars, *eol;
	gchar prefix[] = "\r\nContent-Length: ";
	gsize prefix_size = sizeof(prefix) - 1; /* this sizeof() includes the NULL */
	gsize rest_of_response_size;
	gsize cl_size;
	gsize tmp;

	found_str = g_strstr_len(response->str, response->len, prefix);
	if (!found_str) {
		return 1;
	}
	found_str += prefix_size; /* increment pointer to start desired string */
	rest_of_response_size = response->len - (found_str - response->str);
	eol = g_strstr_len(found_str, rest_of_response_size, "\r\n");
	if (!eol)
	{
		return 1;
	}
	cl_size = eol - found_str + 1;
	purple_debug_info("campfire","content length str len = %lu\n", cl_size);
	purple_debug_info("campfire","%p %p -%c- -%c- \n", found_str, eol, found_str[0], eol[0]);
	str = g_malloc0(cl_size);
	g_strlcpy(str, found_str, cl_size);
	tmp = g_ascii_strtoll(str, &extra_chars, 10);
	g_free(str);
	if (tmp == 0  && extra_chars == str) {
		return 1;
	}
	*cl = tmp;
	return 0;
}

gint campfire_http_response(PurpleSslConnection *gsc, CampfireSslTransaction *xaction, PurpleInputCondition cond,
                            xmlnode **node)
{
	gchar buf[1024];
	gchar *blank_line = "\r\n\r\n",
	      *content_start,
	      *xml_header = "<?xml";
	gint len, errsv = 0;
	gint status;
	gsize size_content_received;

	if (xaction->campfire && !xaction->campfire->gsc)
	{
		purple_debug_info("campfire","somefin aint right.\n");
		return CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED;
	}

	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	if (!xaction->http_response)
	{
		xaction->http_response = g_string_new("");
	}

	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);

	errno = 0;
	while((len = purple_ssl_read(gsc, buf, sizeof(buf))) > 0) {
		purple_debug_info("campfire",
		                  "read %d bytes from HTTP Response, errno: %i\n",
		                  len, errno);
		xaction->http_response = g_string_append_len(xaction->http_response, buf, len);
	}
	errsv = errno;

	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);


	if(len < 0 && errsv == EAGAIN)
	{
		if (xaction->http_response->len == 0)
		{
			purple_debug_info("campfire", "TRY AGAIN\n");
			return CAMPFIRE_HTTP_RESPONSE_STATUS_TRY_AGAIN;
		}
	}
	else if (len == 0)
	{
		purple_debug_info("campfire", "SERVER CLOSED CONNECTION\n");
		if (xaction->http_response->len == 0)
		{
			return CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED;
		}
	}
	else
	{
		purple_debug_info("campfire", "LOST CONNECTION\n");
		purple_debug_info("campfire", "errno: %d\n", errsv);
		if (node) {
			*node = NULL;
		}
		/*purple_connection_error_reason(js->gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, tmp);*/
		return CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION;
	}

	/*
	 * only continue here when xaction->http_response->len >= 0
	 * below we parse the response and pull out the
	 * xml we need
	 */
	//g_string_append(xaction->http_response, "\n");
	purple_debug_info("campfire", "HTTP response size: %lu bytes\n", xaction->http_response->len);
	purple_debug_info("campfire", "HTTP response string:\n%s\n", xaction->http_response->str);

	// look for the status
	if(campfire_get_http_status_from_response(&status, xaction->http_response))
	{
		return CAMPFIRE_HTTP_RESPONSE_STATUS_FAIL;
	}

	// look for the content length
	if(campfire_get_content_length_from_response(&xaction->content_len, xaction->http_response))
	{
		return CAMPFIRE_HTTP_RESPONSE_STATUS_FAIL;
	}
	purple_debug_info("campfire", "HTTP content-length: %lu\n", xaction->content_len);
	
	purple_debug_info("campfire","%s:%d\n", __FUNCTION__, __LINE__);
	
	// look for the content
	content_start = g_strstr_len(xaction->http_response->str, xaction->http_response->len, blank_line);
	if (content_start)
	{
		purple_debug_info("campfire", "content: %s\n", content_start);
	}
	else 
	{
		purple_debug_info("campfire", "no content found\n");
		if (node) {
			*node = NULL;
		}
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_CONTENT;
	}

	content_start += 4 * sizeof(gchar); // account for \r\n\r\n
	size_content_received =    (unsigned long)xaction->http_response->str
	                        +  xaction->http_response->len
	                        - (unsigned long)content_start; // pointer math
	purple_debug_info("campfire", "content-length debug: %lu %lu\n",
	                  xaction->content_len, size_content_received);

	if (size_content_received < xaction->content_len)
	{
		return CAMPFIRE_HTTP_RESPONSE_STATUS_MORE_CONTENT_NEEDED;
	}

	content_start = g_strstr_len(content_start, strlen(content_start), xml_header);

	if(content_start == NULL)
	{
		if(node)
		{
			*node = NULL;
		}
		if(status == 200)
		{
			purple_debug_info("campfire", "no xml found, status OK\n");
			return CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML;
		}
		purple_debug_info("campfire", "no xml found\n");
		return CAMPFIRE_HTTP_RESPONSE_STATUS_NO_XML;
	}

	//purple_debug_info("campfire", "raw xml: %s\n", content_tmp);

	if (node) {
		*node = xmlnode_from_str(content_start, -1);
	}
	g_string_free(xaction->http_response, TRUE);
	xaction->http_response = NULL;
	return CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK;
}

void campfire_ssl_handler(CampfireConn *campfire, PurpleSslConnection *gsc, PurpleInputCondition cond)
{	
	GList *first = g_list_first(campfire->queue);
	CampfireSslTransaction *xaction = NULL;
	gint status;
	gboolean close_ssl = FALSE;
	gboolean cleanup = TRUE;

	if(first)
	{
		xaction = first->data;
	}
	else
	{
		xaction = g_new0(CampfireSslTransaction, 1);
		xaction->campfire = campfire;
	}

	purple_debug_info("campfire", "%s: first: %p\n", __FUNCTION__, first);
	
	status = campfire_http_response(gsc, xaction, cond, &(xaction->xml_response));

	if(status == CAMPFIRE_HTTP_RESPONSE_STATUS_XML_OK ||
	   status == CAMPFIRE_HTTP_RESPONSE_STATUS_OK_NO_XML)
	{
		if (xaction && xaction->response_cb)
		{
			xaction->response_cb(xaction, gsc, cond);
		}
		cleanup = TRUE;
	}
	else if(    status == CAMPFIRE_HTTP_RESPONSE_STATUS_LOST_CONNECTION
	         || status ==  CAMPFIRE_HTTP_RESPONSE_STATUS_DISCONNECTED)
	{
		close_ssl = TRUE;
		cleanup = TRUE;
	}
	else
	{
		cleanup = FALSE;
	}

	if (close_ssl && campfire->gsc)
	{
		purple_debug_info("campfire", "closing ssl connection:%p (%p)\n", gsc, campfire->gsc);
		campfire->gsc = NULL;
		purple_ssl_close(gsc);
		cleanup = TRUE;
	}
			
	if (cleanup)
	{
		if (xaction)
		{
			if (xaction->room_id)
			{
				g_free(xaction->room_id);
			}
			if (xaction->http_request)
			{
				g_string_free(xaction->http_request, TRUE);
			}
			if (xaction->http_response)
			{
				g_string_free(xaction->http_response, TRUE);
			}
			if (xaction->xml_response)
			{
				xmlnode_free(xaction->xml_response);
			}
			g_free(xaction);
		}

		if (first)
		{
			purple_debug_info("campfire", "removing from queue: length: %d\n",
			                  g_list_length(campfire->queue));
			campfire->queue = g_list_remove(campfire->queue, xaction);
			purple_debug_info("campfire", "removed from queue: length: %d\n",
			                  g_list_length(campfire->queue));
		}

		first = g_list_first(campfire->queue);
		if (first)
		{
			xaction = first->data;
			purple_debug_info("campfire", "writing subsequent request on ssl connection\n");
			purple_ssl_write(gsc, xaction->http_request->str, xaction->http_request->len);
		}
	}			
}

void campfire_ssl_connect(CampfireConn *campfire, PurpleInputCondition cond)
{
	GList *first = NULL;
	CampfireSslTransaction *xaction = NULL;

	purple_debug_info("campfire", "%s\n", __FUNCTION__);
	if(!campfire)
	{
		return;
	}
	else
	{
		first = g_list_first(campfire->queue);
	}

	if(!first)
	{
		return;
	}
	else
	{
		xaction = first->data;
	}

	if(!xaction)
	{
		return;
	}

	if(!campfire->gsc)
	{
		purple_debug_info("campfire", "new ssl connection\n");
		campfire->gsc = purple_ssl_connect(campfire->account,
		                                   campfire->hostname,
		                                   443,
		                                   (PurpleSslInputFunction)(campfire_ssl_connect),
		                                   campfire_ssl_failure,
		                                   campfire);
		purple_debug_info("campfire", "new ssl connection kicked off.\n");
	}
	else
	{
		purple_debug_info("campfire", "previous ssl connection\n");
		if(g_list_length(campfire->queue) == 1)
		{
			/* campfire_ssl_handler is the ONLY input handler we EVER use
			 * So... if there is already an input handler present (inpa > 0),
			 * then we DON"T want to add another input handler.  Quite a few hours
			 * spent chasing bugs when multiple input handlers were added!
			 */
			if (campfire->gsc->inpa == 0)
			{
				purple_debug_info("campfire", "adding input\n");
				purple_ssl_input_add(campfire->gsc, (PurpleSslInputFunction)(campfire_ssl_handler), campfire);
			}
			purple_debug_info("campfire", "writing first request on ssl connection\n");
			purple_ssl_write(campfire->gsc, xaction->http_request->str, xaction->http_request->len);
		}
	}
}

void campfire_queue_xaction(CampfireConn *campfire, CampfireSslTransaction *xaction, PurpleInputCondition cond)
{
	purple_debug_info("campfire", "%s input condition: %i\n", __FUNCTION__, cond);
	campfire->queue = g_list_append(campfire->queue, xaction);
	purple_debug_info("campfire", "queue length %d\n", g_list_length(campfire->queue));
	campfire_ssl_connect(campfire, cond);
}
