//
// Client processing code for the Printer Application Framework
//
// Copyright © 2019-2020 by Michael R Sweet.
// Copyright © 2010-2019 by Apple Inc.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pappl-private.h"


//
// 'papplClientCreate()' - Accept a new network connection and create a client object.
//

pappl_client_t *			// O - Client
papplClientCreate(
    pappl_system_t *system,		// I - Printer
    int            sock)		// I - Listen socket
{
  pappl_client_t	*client;	// Client


  if ((client = calloc(1, sizeof(pappl_client_t))) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to allocate memory for client connection: %s", strerror(errno));
    return (NULL);
  }

  client->system = system;

  pthread_rwlock_wrlock(&system->rwlock);
  client->number = system->next_client ++;
  pthread_rwlock_unlock(&system->rwlock);

  // Accept the client and get the remote address...
  if ((client->http = httpAcceptConnection(sock, 1)) == NULL)
  {
    papplLog(system, PAPPL_LOGLEVEL_ERROR, "Unable to accept client connection: %s", strerror(errno));
    free(client);
    return (NULL);
  }

  httpGetHostname(client->http, client->hostname, sizeof(client->hostname));

  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Accepted connection from '%s'.", client->hostname);

  return (client);
}


//
// 'papplClientDelete()' - Close the socket and free all memory used by a client object.
//

void
papplClientDelete(
    pappl_client_t *client)		// I - Client
{
  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Closing connection from '%s'.", client->hostname);

  // Flush pending writes before closing...
  httpFlushWrite(client->http);

  // Free memory...
  httpClose(client->http);

  ippDelete(client->request);
  ippDelete(client->response);

  free(client);
}


//
// '_papplClientProcessHTTP()' - Process a HTTP request.
//

int					// O - 1 on success, 0 on failure
_papplClientProcessHTTP(
    pappl_client_t *client)		// I - Client connection
{
  char			uri[1024];	// URI
  http_state_t		http_state;	// HTTP state
  http_status_t		http_status;	// HTTP status
  ipp_state_t		ipp_state;	// State of IPP transfer
  char			scheme[32],	// Method/scheme
			userpass[128],	// Username:password
			hostname[HTTP_MAX_HOST];
					// Hostname
  int			port;		// Port number
  _pappl_resource_t	*resource;	// Current resource
  static const char * const http_states[] =
  {					// Strings for logging HTTP method
    "WAITING",
    "OPTIONS",
    "GET",
    "GET_SEND",
    "HEAD",
    "POST",
    "POST_RECV",
    "POST_SEND",
    "PUT",
    "PUT_RECV",
    "DELETE",
    "TRACE",
    "CONNECT",
    "STATUS",
    "UNKNOWN_METHOD",
    "UNKNOWN_VERSION"
  };


  // Clear state variables...
  ippDelete(client->request);
  ippDelete(client->response);

  client->request   = NULL;
  client->response  = NULL;
  client->operation = HTTP_STATE_WAITING;

  // Read a request from the connection...
  while ((http_state = httpReadRequest(client->http, uri, sizeof(uri))) == HTTP_STATE_WAITING)
    usleep(1);

  // Parse the request line...
  if (http_state == HTTP_STATE_ERROR)
  {
    if (httpError(client->http) == EPIPE)
      papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Client closed connection.");
    else
      papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "Bad request line (%s).", strerror(httpError(client->http)));

    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_METHOD)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad/unknown operation.");
    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }
  else if (http_state == HTTP_STATE_UNKNOWN_VERSION)
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad HTTP version.");
    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  papplLogClient(client, PAPPL_LOGLEVEL_DEBUG, "%s %s", http_states[http_state], uri);

  // Separate the URI into its components...
  if (httpSeparateURI(HTTP_URI_CODING_MOST, uri, scheme, sizeof(scheme), userpass, sizeof(userpass), hostname, sizeof(hostname), &port, client->uri, sizeof(client->uri)) < HTTP_URI_STATUS_OK && (http_state != HTTP_STATE_OPTIONS || strcmp(uri, "*")))
  {
    papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Bad URI '%s'.", uri);
    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if ((client->options = strchr(client->uri, '?')) != NULL)
    *(client->options)++ = '\0';

  // Process the request...
  client->start     = time(NULL);
  client->operation = httpGetState(client->http);

  // Parse incoming parameters until the status changes...
  while ((http_status = httpUpdate(client->http)) == HTTP_STATUS_CONTINUE);

  if (http_status != HTTP_STATUS_OK)
  {
    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  if (!httpGetField(client->http, HTTP_FIELD_HOST)[0] && httpGetVersion(client->http) >= HTTP_VERSION_1_1)
  {
    // HTTP/1.1 and higher require the "Host:" field...
    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
    return (0);
  }

  // Handle HTTP Upgrade...
  if (!strcasecmp(httpGetField(client->http, HTTP_FIELD_CONNECTION), "Upgrade"))
  {
    if (strstr(httpGetField(client->http, HTTP_FIELD_UPGRADE), "TLS/") != NULL && !httpIsEncrypted(client->http))
    {
      if (!papplClientRespondHTTP(client, HTTP_STATUS_SWITCHING_PROTOCOLS, NULL, NULL, 0))
        return (0);

      papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Upgrading to encrypted connection.");

      if (httpEncryption(client->http, HTTP_ENCRYPTION_REQUIRED))
      {
	papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	return (0);
      }

      papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Connection now encrypted.");
    }
    else if (!papplClientRespondHTTP(client, HTTP_STATUS_NOT_IMPLEMENTED, NULL, NULL, 0))
      return (0);
  }

  // Handle HTTP Expect...
  if (httpGetExpect(client->http) && (client->operation == HTTP_STATE_POST || client->operation == HTTP_STATE_PUT))
  {
    if (httpGetExpect(client->http) == HTTP_STATUS_CONTINUE)
    {
      // Send 100-continue header...
      if (!papplClientRespondHTTP(client, HTTP_STATUS_CONTINUE, NULL, NULL, 0))
	return (0);
    }
    else
    {
      // Send 417-expectation-failed header...
      if (!papplClientRespondHTTP(client, HTTP_STATUS_EXPECTATION_FAILED, NULL, NULL, 0))
	return (0);
    }
  }

  // Handle new transfers...
  switch (client->operation)
  {
    case HTTP_STATE_OPTIONS :
        // Do OPTIONS command...
	return (papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, NULL, 0));

    case HTTP_STATE_HEAD :
        // See if we have a matching resource to serve...
        if ((resource = _papplSystemFindResource(client->system, client->uri)) != NULL)
	  return (papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, resource->format, 0));

        // If we get here the resource wasn't found...
	return (papplClientRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_GET :
        // See if we have a matching resource to serve...
        if ((resource = _papplSystemFindResource(client->system, client->uri)) != NULL)
        {
          if (resource->cb)
          {
            // Send output of a callback...
            return ((resource->cb)(client, resource->cbdata));
	  }
	  else if (resource->filename)
	  {
	    // Send an external file...
	    int		fd;		// Resource file descriptor
	    char	buffer[8192];	// Copy buffer
	    ssize_t	bytes;		// Bytes read/written

            if ((fd = open(resource->filename, O_RDONLY)) >= 0)
	    {
	      if (!papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, resource->format, 0))
		return (0);

              while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
                httpWrite2(client->http, buffer, (size_t)bytes);

	      httpWrite2(client->http, "", 0);

	      close(fd);

	      return (1);
	    }
	  }
	  else
	  {
	    // Send a static resource file...
	    if (!papplClientRespondHTTP(client, HTTP_STATUS_OK, NULL, resource->format, resource->length))
	      return (0);

	    httpWrite2(client->http, (const char *)resource->data, resource->length);
	    httpFlushWrite(client->http);
	    return (1);
	  }
	}

        // If we get here then the resource wasn't found...
	return (papplClientRespondHTTP(client, HTTP_STATUS_NOT_FOUND, NULL, NULL, 0));

    case HTTP_STATE_POST :
        // See if we have a matching resource to serve...
        if ((resource = _papplSystemFindResource(client->system, client->uri)) != NULL)
        {
          if (resource->cb)
          {
            // Handle a post request through the callback...
            return ((resource->cb)(client, resource->cbdata));
          }
          else
          {
            // Otherwise you can't POST to a resource...
	    return (papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
          }
        }
        else if (strcmp(httpGetField(client->http, HTTP_FIELD_CONTENT_TYPE), "application/ipp"))
        {
	  // Not an IPP request...
	  return (papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0));
	}

        // Read the IPP request...
	client->request = ippNew();

        while ((ipp_state = ippRead(client->http, client->request)) != IPP_STATE_DATA)
	{
	  if (ipp_state == IPP_STATE_ERROR)
	  {
            papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "IPP read error (%s).", cupsLastErrorString());
	    papplClientRespondHTTP(client, HTTP_STATUS_BAD_REQUEST, NULL, NULL, 0);
	    return (0);
	  }
	}

        // Now that we have the IPP request, process the request...
        return (_papplClientProcessIPP(client));

    default :
        break; // Anti-compiler-warning-code
  }

  return (1);
}


//
// 'papplClientRespondHTTP()' - Send a HTTP response.
//

int					// O - 1 on success, 0 on failure
papplClientRespondHTTP(
    pappl_client_t *client,		// I - Client
    http_status_t  code,		// I - HTTP status of response
    const char     *content_encoding,	// I - Content-Encoding of response
    const char     *type,		// I - MIME media type of response
    size_t         length)		// I - Length of response
{
  char	message[1024];			// Text message


  papplLogClient(client, PAPPL_LOGLEVEL_INFO, "%s %s %d", httpStatus(code), type, (int)length);

  if (code == HTTP_STATUS_CONTINUE)
  {
    // 100-continue doesn't send any headers...
    return (httpWriteResponse(client->http, HTTP_STATUS_CONTINUE) == 0);
  }

  // Format an error message...
  if (!type && !length && code != HTTP_STATUS_OK && code != HTTP_STATUS_SWITCHING_PROTOCOLS)
  {
    snprintf(message, sizeof(message), "%d - %s\n", code, httpStatus(code));

    type   = "text/plain";
    length = strlen(message);
  }
  else
    message[0] = '\0';

  // Send the HTTP response header...
  httpClearFields(client->http);

  if (code == HTTP_STATUS_METHOD_NOT_ALLOWED || client->operation == HTTP_STATE_OPTIONS)
    httpSetField(client->http, HTTP_FIELD_ALLOW, "GET, HEAD, OPTIONS, POST");

  if (code == HTTP_STATUS_UNAUTHORIZED)
    httpSetField(client->http, HTTP_FIELD_WWW_AUTHENTICATE, "Basic realm=\"LPrint\"");

  if (type)
  {
    if (!strcmp(type, "text/html"))
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE,
                   "text/html; charset=utf-8");
    else
      httpSetField(client->http, HTTP_FIELD_CONTENT_TYPE, type);

    if (content_encoding)
      httpSetField(client->http, HTTP_FIELD_CONTENT_ENCODING, content_encoding);
  }

  httpSetLength(client->http, length);

  if (code == HTTP_STATUS_UPGRADE_REQUIRED && client->operation == HTTP_STATE_GET)
  {
    char	redirect[1024];		// Redirect URI

    code = HTTP_STATUS_MOVED_PERMANENTLY;

    httpAssembleURI(HTTP_URI_CODING_ALL, redirect, sizeof(redirect), "https", NULL, client->system->hostname, client->system->port, client->uri);
    httpSetField(client->http, HTTP_FIELD_LOCATION, redirect);
  }

  if (httpWriteResponse(client->http, code) < 0)
    return (0);

  // Send the response data...
  if (message[0])
  {
    // Send a plain text message.
    if (httpPrintf(client->http, "%s", message) < 0)
      return (0);

    if (httpWrite2(client->http, "", 0) < 0)
      return (0);
  }
  else if (client->response)
  {
    // Send an IPP response...
    papplLogAttributes(client, "Response", client->response, 2);

    ippSetState(client->response, IPP_STATE_IDLE);

    if (ippWrite(client->http, client->response) != IPP_STATE_DATA)
      return (0);
  }

  return (1);
}


//
// '_papplClientRun()' - Process client requests on a thread.
//

void *					// O - Exit status
_papplClientRun(
    pappl_client_t *client)		// I - Client
{
  int first_time = 1;			// First time request?


  // Loop until we are out of requests or timeout (30 seconds)...
  while (httpWait(client->http, 30000))
  {
    if (first_time)
    {
      // See if we need to negotiate a TLS connection...
      char buf[1];			// First byte from client

      if (recv(httpGetFd(client->http), buf, 1, MSG_PEEK) == 1 && (!buf[0] || !strchr("DGHOPT", buf[0])))
      {
        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Starting HTTPS session.");

	if (httpEncryption(client->http, HTTP_ENCRYPTION_ALWAYS))
	{
          papplLogClient(client, PAPPL_LOGLEVEL_ERROR, "Unable to encrypt connection: %s", cupsLastErrorString());
	  break;
        }

        papplLogClient(client, PAPPL_LOGLEVEL_INFO, "Connection now encrypted.");
      }

      first_time = 0;
    }

    if (!_papplClientProcessHTTP(client))
      break;
  }

  // Close the conection to the client and return...
  papplClientDelete(client);

  return (NULL);
}