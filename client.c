#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#include "networking.h"
#include "hashtable.h"

#include "other_code.c"

WINDOW *pad;
WINDOW *w;
static int mrow, mcol;

int main()
{
	// initialize curses
	initscr();
	clear();
	cbreak();
	start_color();

	// get terminal size
	getmaxyx(stdscr, mrow, mcol);
	curs_set(0);

	w = newwin(1, mcol, 0, 0);
	wattron(w, A_REVERSE);
	wprintw(w, " welcome to suckless /g/ -- sucklessg.org \n");
	wattroff(w, A_REVERSE);
	wrefresh(w);

	// makes a request to the server and returns the servers response.
	request_t request;
	request.r = malloc(512);
	request.l = sprintf(request.r, "GET /page/0\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	post_t threads = makeRequest(request);

	// load the hash table
	ht_hash_table **ht = malloc(sizeof(ht_hash_table*) * threads.length);
	for(int i = 0; i < threads.length; ++i)
		ht[i] = ht_new();
	loadContent(ht, threads);

	int rowcount = threads.length * 10;

	// create pad
	pad = newpad (rowcount+2, mcol);
	keypad(pad, TRUE);
	// col titles
	wattron(pad, A_UNDERLINE);
	wprintw(pad, "press UP/DOWN to view.\n");
	wattroff(pad, A_UNDERLINE);
	wrefresh(pad);

	for (int i = 0; i < threads.length; i++) {
		wattron(pad, A_REVERSE);
		wprintw(pad, "[ id: %s ]\n", ht_search(ht[i], "id"));
		wattroff(pad, A_REVERSE);
		wprintw(pad, "%s\n@ %s\n(replies: %s)\n\n",  ht_search(ht[i], "content"),  ht_search(ht[i], "created"),  ht_search(ht[i], "replies"));
	}

	// Show content of pad
	int mypadpos = 0;
	prefresh(pad, mypadpos, 0, 1, 0, mrow-1, mcol);

	// wait for exit key
	int ch;
	rowcount -= mrow-2;
	while((ch = wgetch(pad)) != 'q') {
		switch (ch) {
			case KEY_UP:
				if (mypadpos > 0) {
					mypadpos--;
				}
				prefresh(pad, mypadpos, 0, 1, 0, mrow-1, mcol);
			break;
			case KEY_DOWN:
				if (mypadpos < rowcount+1) {
					mypadpos++;
				}
				prefresh(pad, mypadpos, 0, 1, 0, mrow-1, mcol);
			break;
		}
	}

	// remove window
	delwin(pad);
	delwin(w);
	clear();
	refresh();
	endwin();

	// free memory
	for(int i = 0; i < threads.length; ++i)
		ht_del_hash_table(ht[i]);
	free(ht);

	for(int i = 0; i < threads.length; i++)
		free(threads.objects[i]);
	free(threads.objects);

	return (0);
}
