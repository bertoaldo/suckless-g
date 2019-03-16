/*
	Standard interface with web server.
	sendRequest() sends off a request to the web server.
	It will return the response.

	loc 13-MAR-2019
		initial version.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

/* Extra Libraries
   ===============
	Keeping this to a minimum.
*/
#include <openssl/ssl.h>
#include "jsmn.h"

/* User defined includes
   =====================
	Includes which contain types and
	functions made by me.
*/
#include "networking.h"

post_t parseContent(response_t response)
{
	// Initialize the JSON parser and load up the JSON
	jsmn_parser p;
	jsmn_init(&p);

	int resultCode = 0, n_tokens = BUFFER_SIZE;
	jsmntok_t *tokens = (jsmntok_t *) malloc(sizeof(jsmntok_t) * n_tokens); // Allocate 512 tokens to start with
	while((resultCode = jsmn_parse(&p, response.r, response.l, tokens, n_tokens)) == JSMN_ERROR_NOMEM) {
		n_tokens <<= 1;
		tokens = (jsmntok_t *) realloc(tokens, sizeof(jsmntok_t) * n_tokens);
	}

	// Load JSON objects into char arrays
	int i = 0, object_count = 0;
	for(i = 0; i < resultCode; ++i) if(tokens[i].type == JSMN_OBJECT) object_count++;
	char **objects = (char **) malloc(object_count * sizeof(char **));

	object_count = 0;
	for(i = 0; i < resultCode; ++i) {
		if(tokens[i].type == JSMN_OBJECT) {
			objects[object_count] = (char *) malloc((tokens[i].end - tokens[i].start + 1) * sizeof(char *));
			sprintf(objects[object_count], "%.*s", tokens[i].end-tokens[i].start, response.r+tokens[i].start);
			object_count++;
		}
	}

	// load up the threads into the container.
	post_t threads;
	threads.objects = objects;
	threads.length = object_count;

	// free up memory
	free(tokens);

	return threads;
}

/* Returns response */
response_t sendRequest(int sockfd, request_t request)
{
	response_t response;
	response.l = 0;
	response.r = NULL;

	char buff[BUFFER_SIZE] = {};
	int buff_size = 0;

	SSL_load_error_strings ();
	SSL_library_init ();
	SSL_CTX *ssl_ctx = SSL_CTX_new (SSLv23_client_method ());

	// create an SSL connection and attach it to the socket
	SSL *conn = SSL_new(ssl_ctx);
	SSL_set_fd(conn, sockfd);

	// perform the SSL/TLS handshake with the server - when on the
	// server side, this would use SSL_accept()
	SSL_connect(conn);

	if(SSL_write(conn, request.r, request.l) >= 0) {
		while((buff_size = SSL_read(conn, buff, BUFFER_SIZE)) > 0) {
			buff[buff_size] = '\0';
			buff_size++;
			if(response.l == 0) {
				response.r = (char *) malloc(buff_size);
				response.l += buff_size;
				strcpy(response.r, buff);
			} else {
				response.l += buff_size;
				response.r = (char *) realloc(response.r, response.l);
				strcat(response.r, buff);
			}
		}
	} else {
		perror("Failed to send request");
	}

	SSL_free(conn);
	SSL_CTX_free(ssl_ctx);

	return response;
}

post_t makeRequest(request_t request)
{
	// Initialize memory for the connection
	struct addrinfo hints, *res;
	int sockfd = 0;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	// Establish connection to remote server
	if (getaddrinfo(ADDRESS_URL, "https", &hints, &res) != 0) {
		perror("Error getting address info");
	}
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		perror("Error establishing socket");
	}
	if (connect(sockfd, res->ai_addr, res->ai_addrlen) != 0) {
		perror("Error connecting to socket");
	}

	// Connected! now we can proceed to do what we want.
	response_t response;
	response = sendRequest(sockfd, request);

	// CLose connection and free up memory here
	close(sockfd);
	freeaddrinfo(res);
	free(request.r);

	post_t threads = parseContent(response);
	free(response.r);

	return threads;
}
