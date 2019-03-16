#ifndef NETWORKING_H
#define NETWORKING_H

#define ADDRESS_URL "sucklessg.org"
#define BUFFER_SIZE 512

typedef struct {
	char *r;
	int l;
} response_t;
typedef response_t request_t;

typedef struct {
	char **objects;
	int length;
} post_t;

post_t makeRequest(request_t request);

#endif