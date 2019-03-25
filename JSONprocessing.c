#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "JSONprocessing.h"

static int
objectCount(const char *jsonString)
{
	int c = 0;
	for(const char *ptr = jsonString; *ptr != '\0'; ptr++) {
		if(*ptr == '{') c++;
	}
	return c;
}

static void
unescape(char *string)
{
	char *src, *dst;
	for(src = dst = string; *src != '\0'; ++src) {
		if (*src == '\\' && *(src+1) == 'n') {
			*dst = '\n';
			src++;
		} else {
			*dst = *src;
		}
		if(*dst != '\\') {
			dst++;
			continue;
		}
	}
	*dst = '\0';
}

post_t
parseContent(response_t response)
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

int
loadContent(ht_hash_table **ht, post_t response)
{
	int resultCode, n_tokens = BUFFER_SIZE;
	jsmn_parser p;
	jsmntok_t *tokens;
	jsmn_init(&p);

	int i = 0;
	char *leaf;
	char *parent;
	for(i = 0; i < response.length; ++i) {
		jsmn_init(&p);
		tokens = (jsmntok_t *) malloc(sizeof(jsmntok_t) * n_tokens); // Allocate 512 tokens to start with
		while((resultCode = jsmn_parse(&p, response.objects[i], strlen(response.objects[i]), tokens, n_tokens)) == JSMN_ERROR_NOMEM) {
			n_tokens <<= 1;
			tokens = (jsmntok_t *) realloc(tokens, sizeof(jsmntok_t) * n_tokens);
		}
		for(int j = 0; j < resultCode; ++j) {
			if(tokens[j].size == 0){
				leaf = (char *) malloc(tokens[j].end-tokens[j].start+1);
				sprintf(leaf, "%.*s", tokens[j].end-tokens[j].start, response.objects[i]+tokens[j].start);
				unescape(leaf);
				parent = (char *) malloc(tokens[j-1].end-tokens[j-1].start+1);
				sprintf(parent, "%.*s", tokens[j-1].end-tokens[j-1].start, response.objects[i]+tokens[j-1].start);

				ht_insert(ht[i], parent, leaf);

				free(leaf);
				free(parent);
			}
		}
		free(tokens);
	}

	return i;
}
