#ifndef JSON_PROCESSING_H
#define JSON_PROCESSING_H

#include "jsmn.h"
#include "hashtable.h"
#include "networking.h"

post_t parseContent(response_t response);
int loadContent(ht_hash_table **ht, post_t response);

#endif
