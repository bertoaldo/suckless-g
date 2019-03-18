#include <ncurses.h>
#include <stdlib.h>
#include <string.h>

#include "networking.h"
#include "hashtable.h"
#include "JSONprocessing.h"

WINDOW *pad;
WINDOW *w;
static int mrow, mcol;

ht_hash_table **ht;
static int ht_length = 0;

void displayHelp() {
	int x = 42;
	int y = 8;
	int offset_x = x / 2;
	int offset_y = y / 2;
	WINDOW *help = newwin(y, x, LINES/2 - offset_y, COLS/2 - offset_x);
	int ch;

	wprintw(help, "\n");
	wattron(help, A_UNDERLINE);
	wprintw(help, " Controls (Press [?] to close)\n");
	wattroff(help, A_UNDERLINE);
	wprintw(help, " UP/DOWN\tNavigate through threads\n");
	wprintw(help, " RIGHT\t\tView thread\n");
	wprintw(help, " LEFT\t\tGo back\n");
	wprintw(help, " R\t\tReply/post new thread\n");
	wprintw(help, " Q\t\tQuit the program\n");
	wprintw(help, "\n");
	box(help, 0, 0);
	wrefresh(help);
	while((ch = wgetch(pad)) != '?') {
		// we can do something here later maybe
	}
	wborder(help, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(help);
	delwin(help);
}

int loadThread(int ht_length, int thread_pos) {
	request_t request;
	request.r = malloc(512);
	if(thread_pos == -1) {
		request.l = sprintf(request.r, "GET /page/0\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	} else {
		request.l = sprintf(request.r, "GET /post/%s\r\nHost: %s\r\n\r\n", ht_search(ht[thread_pos], "id"), ADDRESS_URL);
	}
	post_t posts = makeRequest(request);

	for(int i = 0; i < ht_length; ++i)
		ht_del_hash_table(ht[i]);
	ht = realloc(ht, sizeof(ht_hash_table*) * posts.length);
	for(int i = 0; i < posts.length; ++i)
		ht[i] = ht_new();
	loadContent(ht, posts);

	return posts.length;
}

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
	ht_length = threads.length;

	// load the hash table
	ht = malloc(sizeof(ht_hash_table*) * ht_length);
	for(int i = 0; i < ht_length; ++i)
		ht[i] = ht_new();
	loadContent(ht, threads);

	int rowcount = ht_length * 10;
	int thread_pos = 0;
	int thread_pos_old = 0;
	int attr = 0;
	int depth = 0;
	int *thread_loc = malloc(sizeof(int) * ht_length);
	int x = 0, y = 0;

	// create pad
	pad = newpad (rowcount+2, mcol);
	keypad(pad, TRUE);
	// col titles
	wattron(pad, A_UNDERLINE);
	wprintw(pad, "Press [?] for help\n");
	wattroff(pad, A_UNDERLINE);
	wrefresh(pad);

	for (int i = 0; i < ht_length; i++) {
		attr = (i == thread_pos) ? A_REVERSE | A_BLINK | A_BOLD : A_REVERSE;
		getyx(pad, y, x);
		thread_loc[i] = y - 2;
		wattron(pad, attr);
		if(i == thread_pos) {
			wprintw(pad, ">[ id: %s ]\n", ht_search(ht[i], "id"));
		} else {
			wprintw(pad, "[ id: %s ]\n", ht_search(ht[i], "id"));
		}
		wattroff(pad, attr);
		wprintw(pad, "%s\n@ %s\n(replies: %s)\n\n",  ht_search(ht[i], "content"),  ht_search(ht[i], "created"),  ht_search(ht[i], "replies"));
	}

	// Show content of pad
	prefresh(pad, 0, 0, 1, 0, mrow-1, mcol);

	// wait for exit key
	int ch;
	rowcount -= mrow-2;
	while((ch = wgetch(pad)) != 'q') {
		switch (ch) {
			case KEY_UP:
					thread_pos -= (thread_pos == 0) ? 0 : 1;
			break;
			case KEY_DOWN:
					thread_pos += (thread_pos == ht_length - 1) ? 0 : 1;
			break;
			case KEY_RIGHT:
				// load the thread
				if (depth == 0) {
					ht_length = loadThread(ht_length, thread_pos);
					thread_pos_old = thread_pos;
					thread_pos = 0;
					free(thread_loc);
					thread_loc = malloc(sizeof(int) * ht_length);
					depth++;
				}
			break;
			case KEY_LEFT:
				// back up to the overview
				if (depth == 1) {
					ht_length = loadThread(ht_length, -1);
					thread_pos = thread_pos_old;
					free(thread_loc);
					thread_loc = malloc(sizeof(int) * ht_length);
					depth--;
				}
			break;
			case 'r':
			case 'R':
					//reply function
				break;
			case '?':
					displayHelp();
				break;
		}
		wclear(pad);
		wattron(pad, A_UNDERLINE);
		wprintw(pad, "Press [?] for help\n");
		wattroff(pad, A_UNDERLINE);
		wrefresh(pad);
		for (int i = 0; i < ht_length; i++) {
			getyx(pad, y, x);
			thread_loc[i] = y - 1;
			attr = (i == thread_pos) ? A_REVERSE | A_BLINK | A_BOLD : A_REVERSE;
			wattron(pad, attr);
			if(i == thread_pos) {
				wprintw(pad, ">[ id: %s ]\n", ht_search(ht[i], "id"));
			} else {
				wprintw(pad, "[ id: %s ]\n", ht_search(ht[i], "id"));
			}
			wattroff(pad, attr);
			wprintw(pad, "%s\n@ %s\n(replies: %s)\n\n",  ht_search(ht[i], "content"),  ht_search(ht[i], "created"),  ht_search(ht[i], "replies"));
		}
		prefresh(pad, thread_loc[thread_pos], 0, 1, 0, mrow-1, mcol);
	}

	// remove window
	delwin(pad);
	delwin(w);
	clear();
	refresh();
	endwin();

	// free memory
	for(int i = 0; i < ht_length; ++i)
		ht_del_hash_table(ht[i]);
	free(ht);

	for(int i = 0; i < threads.length; i++)
		free(threads.objects[i]);
	free(threads.objects);

	return (0);
}
