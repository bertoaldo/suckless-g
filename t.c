#include "sucklessg.c"

int main()
{
	post_t threads = makeRequest("GET", "/page/0");

	for(int i = 0; i < threads.length; i++) {
		printf("%s\n", threads.objects[i]);
		free(threads.objects[i]);
	}
	free(threads.objects);

	return 0;
}
