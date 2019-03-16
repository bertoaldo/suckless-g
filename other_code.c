#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jsmn.h"
#include "hashtable.h"
#include "networking.h"

int objectCount(const char *jsonString)
{
	int c = 0;
	for(const char *ptr = jsonString; *ptr != '\0'; ptr++) {
		if(*ptr == '{') c++;
	}
	return c;
}

void unescape(char *string)
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

int loadContent(ht_hash_table **ht, post_t response)
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
