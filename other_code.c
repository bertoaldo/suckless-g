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

// don't use this just yet.
int displayPosts(char **objects, int count)
{
	int resultCode, n_tokens = BUFFER_SIZE;
	jsmn_parser p;
	jsmntok_t *tokens;
	jsmn_init(&p);

	int i = 0;
	char *placeholder;

	FILE *captcha;
	char parent[32];

	for(i = 0; i < count; ++i) {
		jsmn_init(&p);
		jsmntok_t *tokens = (jsmntok_t *) malloc(sizeof(jsmntok_t) * n_tokens); // Allocate 512 tokens to start with
		while((resultCode = jsmn_parse(&p, objects[i], strlen(objects[i]), tokens, n_tokens)) == JSMN_ERROR_NOMEM) {
			n_tokens <<= 1;	
			tokens = (jsmntok_t *) realloc(tokens, sizeof(jsmntok_t) * n_tokens);
		}
		for(int j = 0; j < resultCode; ++j) {
			if(tokens[j].size == 0){
				placeholder = (char *) malloc(tokens[j].end-tokens[j].start+1);
				sprintf(placeholder, "%.*s", tokens[j].end-tokens[j].start, objects[i]+tokens[j].start);
				unescape(placeholder);

				sprintf(parent, "%.*s", tokens[j-1].end-tokens[j-1].start, objects[i]+tokens[j-1].start);
				if(strcmp(parent, "captcha_svg") == 0) {
					captcha = fopen("captcha.svg", "w");
					fprintf(captcha, "%s", placeholder);
					fclose(captcha);
					system("display captcha.svg &");
				} else {
					printf("%.*s: %s\n", tokens[j-1].end-tokens[j-1].start, objects[i]+tokens[j-1].start, placeholder);
				}
				free(placeholder);
			}
		}
		free(tokens);
		printf("\n");
	}

	return i;
}

int parseContent(const response_t response)
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
	char **objects = (char **) malloc(objectCount(response.r));

	int i = 0, object_count = 0;
	for(i = 0; i < resultCode; ++i) {
		if(tokens[i].type == JSMN_OBJECT) {
			objects[object_count] = (char *) malloc(tokens[i].end - tokens[i].start + 1);
			sprintf(objects[object_count++], "%.*s", tokens[i].end-tokens[i].start, response.r+tokens[i].start);
		}
	}

	// Display the contents onto the screen
	displayPosts(objects, object_count);

	// free up memory
	for(i = 0; i < object_count; i++) {
		free(objects[i]);
	}
	free(objects);
	free(tokens);

	return object_count;
}
