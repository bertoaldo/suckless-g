#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <form.h>
#include <ctype.h>

#include "networking.h"
#include "hashtable.h"
#include "JSONprocessing.h"

WINDOW *threadView;
WINDOW *headerView;
WINDOW *replyView;

FORM *form;
FIELD *field[8];

static int mrow, mcol;

ht_hash_table **ht;
static int ht_length = 0;

char* loadCaptcha() {
	ht_hash_table *tmp_ht;
	request_t request;

	request.r = malloc(512);
	request.l = sprintf(request.r, "GET /captcha\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	post_t posts = makeRequest(request);

	tmp_ht = ht_new();
	loadContent(&tmp_ht, posts);

	// write captcha solution to svg file.
	FILE *svg = fopen("captcha.svg", "w");
	fprintf(svg, "%s", ht_search(tmp_ht, "captcha_svg"));
	fclose(svg);
	system("display captcha.svg &");

	// save captcha id into char pointer to return
	char *captcha_id = malloc(strlen(ht_search(tmp_ht, "captcha_id")) + 1);
	strcpy(captcha_id, ht_search(tmp_ht, "captcha_id"));

	ht_del_hash_table(tmp_ht);

	return captcha_id;
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

static char* trim_whitespaces(char *str) {
	char *end;

	// trim leading space
	while(isspace(*str))
		str++;

	if(*str == 0) // all spaces?
		return str;

	// trim trailing space
	end = str + strnlen(str, 128) - 1;

	while(end > str && isspace(*end))
		end--;

	// write new null terminator
	*(end+1) = '\0';

	return str;
}

void displayReply(int depth) {
	// set up the windows to display thread
	// the reply form
	int width = COLS, height = 11;
	int top = LINES - height, left = COLS - width;
	replyView  = newwin(height, COLS, top, left);
	keypad(replyView, TRUE);
	curs_set(1);

	int r, w;

	/* Initialize the fields */
	field[0] = new_field(1, 10, 0, 1, 0, 0);
	field[1] = new_field(1, 4, 0, 12, 0, 0);
	field[2] = new_field(1, 10, 2, 1, 0, 0);
	field[3] = new_field(3, 60, 2, 12, 0, 0);
	field[4] = new_field(1, 10, 6, 1, 0, 0);
	field[5] = new_field(1, 40, 6, 12, 0, 0);
	field[6] = new_field(1, 10, 8, 12, 0, 0);

	/* Set field labels */
	set_field_buffer(field[0], 0, "Captcha:");
	set_field_buffer(field[2], 0, "Content:");
	set_field_buffer(field[4], 0, "Pic:");
	set_field_buffer(field[6], 0, "Done");

	/* Set field options */
	set_field_opts(field[0], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	//field_opts_off(field[1], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
	set_field_opts(field[2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(field[3], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_WRAP);
	set_field_opts(field[4], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(field[5], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);
	field_opts_off(field[6], O_EDIT);

	set_field_back(field[1], A_UNDERLINE);
	set_field_back(field[3], A_UNDERLINE);
	set_field_back(field[5], A_UNDERLINE);

	// Initialize the form
	form = new_form(field);
	scale_form(form, &r, &w);
	set_form_win(form, replyView);
	set_form_sub(form, derwin(replyView, r, w, 1, 1)); // derpad??

	// display the form
	box(replyView, 0, 0);
	wattron(replyView, A_REVERSE);
	if(depth == 0)
		mvwprintw(replyView, 0, 2, "Create new thread");
	else
		mvwprintw(replyView, 0, 2, "Reply to thread");
	wattroff(replyView, A_REVERSE);
	post_form(form);
	wrefresh(replyView);

	// initialize the form windows
	char *captcha_id = loadCaptcha();

	int ch = 0;
	while ((ch = wgetch(replyView)) != KEY_F(1)) {
		switch(ch) {
			case KEY_DOWN:
				form_driver(form, REQ_NEXT_FIELD);
				form_driver(form, REQ_END_LINE);
				break;
			case KEY_UP:
				form_driver(form, REQ_PREV_FIELD);
				form_driver(form, REQ_END_LINE);
				break;
			case KEY_LEFT:
				form_driver(form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				form_driver(form, REQ_NEXT_CHAR);
				break;
			// Delete the char before cursor
			case KEY_BACKSPACE:
			case 127:
				form_driver(form, REQ_DEL_PREV);
				break;
			// Delete the char under the cursor
			case KEY_DC:
				form_driver(form, REQ_DEL_CHAR);
			break;
			default:
				form_driver(form, ch);
				break;
		}
		if(ch == KEY_F(2)) break;
		wrefresh(replyView);
	}

	form_driver(form, REQ_NEXT_FIELD);
	form_driver(form, REQ_PREV_FIELD);

	// make post request to the server.
	if (ch == KEY_F(1)) {
		request_t request;
		request.r = malloc(2048);
		char *body = malloc(2048);
		sprintf(body, "{\n	\"content\":\"%s\",\n	\"captcha_id\": \"%s\",\n	\"captcha_solution\": \"%s\",\n	\"on_thread\": \"%s\",\n	\"pic\": \"%s\"\n}", trim_whitespaces(field_buffer(field[3], 0)), captcha_id, field_buffer(field[1], 0), ((depth == 1) ? ht_search(ht[0], "id") : "-1"), trim_whitespaces(field_buffer(field[5], 0)));
		request.l = sprintf(request.r, "POST /post HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\ncache-control: no-cache\r\n\r\n%s", ADDRESS_URL, (int) strlen(body), body);
		post_t posts = makeRequest(request);
	}

	// free memory
	unpost_form(form);
	free_form(form);
	free_field(field[0]);
	free_field(field[1]);
	free_field(field[2]);
	free_field(field[3]);
	free_field(field[4]);
	free_field(field[5]);
	free_field(field[6]);
	free(captcha_id);

	curs_set(0);
	delwin(replyView);
	//loadThread(ht_length, 0);
}

void displayHelp() {
	int x = 42;
	int y = 8;
	int offset_x = x / 2;
	int offset_y = y / 2;
	WINDOW *help = newwin(y, x, LINES/2 - offset_y, COLS/2 - offset_x);
	int ch;

	// TODO: make this print into a subwin -- use der win
	wprintw(help, "\n");
	wattron(help, A_UNDERLINE);
	wprintw(help, " Controls (Press [?] to close)\n");
	wattroff(help, A_UNDERLINE);
	wprintw(help, " UP/DOWN\tNavigate through threads\n");
	wprintw(help, " RIGHT\t\tView thread\n");
	wprintw(help, " LEFT\t\tGo back\n");
	wprintw(help, " R\t\tReply/make new thread\n");
	wprintw(help, " Q\t\tQuit the program\n");
	wprintw(help, "\n");
	box(help, 0, 0);
	wrefresh(help);
	while((ch = wgetch(threadView)) != '?') {
		// we can do something here later maybe
	}
	wborder(help, ' ', ' ', ' ',' ',' ',' ',' ',' ');
	wrefresh(help);
	delwin(help);
}

int main()
{
	// initialize curses
	initscr();
	clear();
	cbreak();
	start_color();
	noecho();

	// get terminal size
	getmaxyx(stdscr, mrow, mcol);
	curs_set(0);

	headerView = newwin(1, mcol, 0, 0);
	wattron(headerView, A_REVERSE);
	wprintw(headerView, " welcome to suckless /g/ -- sucklessg.org \n");
	wattroff(headerView, A_REVERSE);
	wrefresh(headerView);

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
	threadView = newpad (rowcount+2, mcol);
	keypad(threadView, TRUE);
	wattron(threadView, A_UNDERLINE);
	wprintw(threadView, "Press [?] for help\n");
	wattroff(threadView, A_UNDERLINE);
	wrefresh(threadView);

	for (int i = 0; i < ht_length; i++) {
		attr = (i == thread_pos) ? A_REVERSE | A_BOLD : A_REVERSE;
		getyx(threadView, y, x);
		thread_loc[i] = y - 2;
		wattron(threadView, attr);
		if(i == thread_pos) {
			wprintw(threadView, ">[ id: %s ]\n", ht_search(ht[i], "id"));
		} else {
			wprintw(threadView, "[ id: %s ]\n", ht_search(ht[i], "id"));
		}
		wattroff(threadView, attr);
		wprintw(threadView, "%s\n", ht_search(ht[i], "content"));
		wprintw(threadView, "@ %s\n", ht_search(ht[i], "created"));
		if (strcmp(ht_search(ht[i], "pic"), "") != 0) {
			wprintw(threadView, "pic: %s\n", ht_search(ht[i], "pic"));
		}
		if (depth == 0) {
			wprintw(threadView, "(replies: %s)\n",  ht_search(ht[i], "replies"));
		}
		wprintw(threadView, "\n");
	}

	// Show content of pad
	prefresh(threadView, 0, 0, 1, 0, mrow-1, mcol);

	// wait for exit key
	int ch;
	rowcount -= mrow-2;
	while((ch = wgetch(threadView)) != 'q') {
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
					displayReply(depth);
				break;
			case '?':
					displayHelp();
				break;
		}
		wclear(threadView);
		wattron(threadView, A_UNDERLINE);
		wprintw(threadView, "Press [?] for help\n");
		wattroff(threadView, A_UNDERLINE);
		wrefresh(threadView);
		for (int i = 0; i < ht_length; i++) {
			getyx(threadView, y, x);
			thread_loc[i] = y - 1;
			attr = (i == thread_pos) ? A_REVERSE | A_BLINK | A_BOLD : A_REVERSE;
			wattron(threadView, attr);
			if(i == thread_pos) {
				wprintw(threadView, ">[ id: %s ]\n", ht_search(ht[i], "id"));
			} else {
				wprintw(threadView, "[ id: %s ]\n", ht_search(ht[i], "id"));
			}
			wattroff(threadView, attr);
			wprintw(threadView, "%s\n", ht_search(ht[i], "content"));
			wprintw(threadView, "@ %s\n", ht_search(ht[i], "created"));
			if (strcmp(ht_search(ht[i], "pic"), "") != 0) {
				wprintw(threadView, "pic: %s\n", ht_search(ht[i], "pic"));
			}
			if (depth == 0) {
				wprintw(threadView, "(replies: %s)\n",  ht_search(ht[i], "replies"));
			}
			wprintw(threadView, "\n");
		}
		prefresh(threadView, thread_loc[thread_pos], 0, 1, 0, mrow-1, mcol);
	}

	// remove window
	delwin(threadView);
	delwin(headerView);
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
