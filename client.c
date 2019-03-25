#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <form.h>
#include <menu.h>
#include <ctype.h>
#include <locale.h>
#include <unistd.h>

#include "networking.h"
#include "hashtable.h"
#include "JSONprocessing.h"

WINDOW *threadView;
WINDOW *headerView;
WINDOW *replyView;

FORM *form;
FIELD *field[7];
MENU *menu;
ITEM *item[3];

static int mrow, mcol;
bool is_on_button;

ht_hash_table **ht;
int ht_length = 0;

static post_t
requestWrapper(request_t request)
{
	int x = 17;
	int y = 1;
	WINDOW *w_win = newwin(y, x, 0, COLS - x);
	wprintw(w_win, "[Loading...     ]");
	wrefresh(w_win);

	post_t q = makeRequest(request);

	wclear(w_win);
	wprintw(w_win, "[Loading... Done]");
	wrefresh(w_win);
	delwin(w_win);

	return q;
}

static char*
loadCaptcha()
{
	ht_hash_table *tmp_ht;
	request_t request;

	request.r = malloc(BUFFER_SIZE);
	request.l = sprintf(request.r, "GET /captcha\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	post_t posts = requestWrapper(request);

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
	for(int i = 0; i < posts.length; i++)
		free(posts.objects[i]);
	free(posts.objects);

	return captcha_id;
}

static int
loadThread(int ht_length, int thread_pos)
{
	request_t request;
	request.r = malloc(BUFFER_SIZE);
	if(thread_pos == -1) {
		request.l = sprintf(request.r, "GET /page/0\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	} else {
		request.l = sprintf(request.r, "GET /post/%s\r\nHost: %s\r\n\r\n", ht_search(ht[thread_pos], "id"), ADDRESS_URL);
	}
	post_t posts = requestWrapper(request);

	for(int i = 0; i < ht_length; ++i)
		ht_del_hash_table(ht[i]);
	ht = realloc(ht, sizeof(ht_hash_table*) * posts.length);
	for(int i = 0; i < posts.length; ++i)
		ht[i] = ht_new();
	loadContent(ht, posts);

	// free up dynamic memory
	for(int i = 0; i < posts.length; i++)
		free(posts.objects[i]);
	free(posts.objects);

	return posts.length;
}

static char*
trim_whitespaces(char *str)
{
	char *dst, *src;
	dst = src = str;
	while (*src != '\0')
   {
       while (*src == ' ' && *(src + 1) == ' ')
           src++;
      *dst++ = *src++;
   }
	*dst = '\0';

	return str;
}

static void
displayReply(int depth)
{
	// set up the windows to display thread
	// the reply form
	int width = COLS, height = 11;
	int top = LINES - height, left = COLS - width;
	int r, w;
	is_on_button = false;
	bool post = false;
	replyView  = newwin(height, COLS, top, left);
	keypad(replyView, TRUE);
	curs_set(1);

	/* Initialize the fields */
	field[0] = new_field(1, 10, 0, 1, 0, 0);
	field[1] = new_field(1, 4, 0, 12, 0, 0);
	field[2] = new_field(1, 10, 2, 1, 0, 0);
	field[3] = new_field(3, 60, 2, 12, 0, 0);
	field[4] = new_field(1, 10, 6, 1, 0, 0);
	field[5] = new_field(1, 40, 6, 12, 0, 0);

	item[0] = new_item("[  Post  ]", "");
	item[1] = new_item("[ Cancel ]", "");

	/* Set field labels */
	set_field_buffer(field[0], 0, "Captcha:");
	set_field_buffer(field[2], 0, "Content:");
	set_field_buffer(field[4], 0, "Pic:");

	/* Set field options */
	set_field_opts(field[0], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(field[2], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(field[3], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE | O_WRAP);
	set_field_opts(field[4], O_VISIBLE | O_PUBLIC | O_AUTOSKIP);
	set_field_opts(field[5], O_VISIBLE | O_PUBLIC | O_EDIT | O_ACTIVE);

	set_field_back(field[1], A_UNDERLINE);
	set_field_back(field[3], A_UNDERLINE);
	set_field_back(field[5], A_UNDERLINE);

	// Initialize the form
	form = new_form(field);
	scale_form(form, &r, &w);
	WINDOW *form_win = derwin(replyView, r, w, 1, 1);
	set_form_win(form, form_win);
	set_form_sub(form, derwin(form_win, r, w, 1, 1));

	menu = new_menu(item);
	set_menu_format(menu, 1, 2);
	scale_menu(menu, &r, &w);
	WINDOW *menu_win = derwin(replyView, r, w, 9, 1);
	set_menu_win(menu, menu_win);
	set_menu_sub(menu, derwin(menu_win, r, w, 1, 1));
	set_menu_mark(menu, "");
	pos_menu_cursor(menu);
	set_menu_fore(menu, A_NORMAL);

	// display the form
	box(replyView, 0, 0);
	wattron(replyView, A_REVERSE);
	if(depth == 0)
		mvwprintw(replyView, 0, 2, "Create new thread");
	else
		mvwprintw(replyView, 0, 2, "Reply to thread");
	wattroff(replyView, A_REVERSE);

	post_form(form);
	post_menu(menu);
	wrefresh(replyView);
	wrefresh(form_win);
	wrefresh(menu_win);

	// initialize the form windows
	char *captcha_id = loadCaptcha();
	wrefresh(replyView);

	int ch = 0;
	while ((ch = wgetch(replyView)) != KEY_F(1)) {
		switch(ch) {
			case KEY_DOWN:
					form_driver(form, REQ_NEXT_LINE);
				break;
			case KEY_UP:
					form_driver(form, REQ_PREV_LINE);
				break;
			case KEY_LEFT:
				if (is_on_button)
					menu_driver(menu, REQ_LEFT_ITEM);
				else
					form_driver(form, REQ_PREV_CHAR);
				break;
			case KEY_RIGHT:
				if (is_on_button)
					menu_driver(menu, REQ_RIGHT_ITEM);
				else
					form_driver(form, REQ_NEXT_CHAR);
				break;
			// Delete the char before cursor
			case KEY_BACKSPACE:
			case 127:
				form_driver(form, REQ_DEL_PREV);
				break;
			case KEY_ENTER:
			case 10:
				if (is_on_button) {
					const char *name = item_name(current_item(menu));

					if (strcmp(name, item_name(item[0])) == 0) {
						post = true;
					} else if (strcmp(name, item_name(item[1])) == 0) {
						post = false;
					}
				} else {
					form_driver(form, '\n');
					form_driver(form, REQ_NEW_LINE);
				}
				break;
			// Delete the char under the cursor
			case KEY_DC:
				form_driver(form, REQ_DEL_CHAR);
			break;
			case 9:
					if (form->current == field[form->maxfield-1] && !is_on_button) {
						// Those 2 lines allow the field buffer to be set
						form_driver(form, REQ_PREV_FIELD);
						form_driver(form, REQ_NEXT_FIELD);

						menu_driver(menu, REQ_FIRST_ITEM);
						is_on_button = true;
						set_menu_fore(menu, A_REVERSE);
						pos_menu_cursor(menu);
						curs_set(0);
					} else {
						form_driver(form, REQ_NEXT_FIELD);
						form_driver(form, REQ_BEG_LINE);

						is_on_button = false;
						set_menu_fore(menu, A_NORMAL);
						pos_form_cursor(form);
						curs_set(1);
					}
				break;
			default:
				form_driver(form, ch);
				break;
		}
		if(ch == 10 && is_on_button) break;
		wrefresh(replyView);
		wrefresh(menu_win);
		wrefresh(form_win);
	}

	// make post request to the server.
	if (post) {
		form_driver(form, REQ_NEXT_FIELD);
		form_driver(form, REQ_PREV_FIELD);
		request_t request;
		request.r = malloc(2048);
		char *body = malloc(2048);
		if (depth == 1)
			sprintf(body, "{\n	\"content\":\"%s\",\n	\"captcha_id\": \"%s\",\n	\"captcha_solution\": \"%s\",\n	\"on_thread\": \"%s\",\n	\"pic\": \"%s\"\n}", trim_whitespaces(field_buffer(field[3], 0)), captcha_id, field_buffer(field[1], 0), ht_search(ht[0], "id"), trim_whitespaces(field_buffer(field[5], 0)));
		else
			sprintf(body, "{\n	\"content\":\"%s\",\n	\"captcha_id\": \"%s\",\n	\"captcha_solution\": \"%s\",\n	\"pic\": \"%s\"\n}", trim_whitespaces(field_buffer(field[3], 0)), captcha_id, field_buffer(field[1], 0), trim_whitespaces(field_buffer(field[5], 0)));
		request.l = sprintf(request.r, "POST /post HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\ncache-control: no-cache\r\n\r\n%s", ADDRESS_URL, (int) strlen(body), body);
		post_t posts = requestWrapper(request);

		FILE *delete_codes = fopen("post_hist.txt", "a");
		for(int i = 0; i < posts.length; i++) {
			fprintf(delete_codes, "%s", posts.objects[i]);
			if (i != posts.length - 1)
				fprintf(delete_codes, ",");
		}
		fclose(delete_codes);

		free(body);
		for(int i = 0; i < posts.length; i++)
			free(posts.objects[i]);
		free(posts.objects);
	}

	// free memory
	unpost_form(form);
	unpost_menu(menu);
	free_form(form);
	free_menu(menu);
	free_field(field[0]);
	free_field(field[1]);
	free_field(field[2]);
	free_field(field[3]);
	free_field(field[4]);
	free_field(field[5]);
	free_item(item[0]);
	free_item(item[1]);
	free(captcha_id);

	curs_set(0);
	delwin(menu_win);
	delwin(form_win);
	delwin(replyView);
}

static void
displayHelp()
{
	int x = 42;
	int y = 10;
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
	wprintw(help, " r\t\tRefresh page\n");
	wprintw(help, " D\t\tShow your previous posts\n");
	wprintw(help, " ESC\t\tQuit the program\n");
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

static void
displayDelete()
{
	FILE *fd = fopen("post_hist.txt", "r");

	// get file size
	fseek(fd, 0, SEEK_END);
	int sz = ftell(fd);
	rewind(fd);

	char *f = malloc(sz+1);
	fread(f, sizeof(char), sz, fd);
	f[sz] = '\0';
	response_t ph;
	ph.r = malloc(sz + 3);
	ph.l = sprintf(ph.r, "[%s]", f);
	ph.r[ph.l] = '\0';

	post_t p = parseContent(ph);

	ht_hash_table **del_ht = malloc(sizeof(ht_hash_table *) * p.length);
	for(int i = 0; i < p.length; i++)
		del_ht[i] = ht_new();
	loadContent(del_ht, p);

	int width = 50, height = 15;
	int top = height, left = width;
	int x = (COLS - left) / 2, y = (LINES - top) / 2;
	WINDOW *del_win = newwin(height, width, y, x);
	keypad(del_win, TRUE);
	box(del_win, 0, 0);
	wattron(del_win, A_REVERSE);
	mvwprintw(del_win, 0, 2, "Delete a post?");
	wattroff(del_win, A_REVERSE);
	wrefresh(del_win);

	WINDOW *del_posts = subwin(del_win, height - 2, width - 2, y+1, x+1);
	int pos = 0, ch = 0;
	do {
		wclear(del_posts);
		switch (ch) {
			case KEY_UP:
					pos -= (pos > 0) ? 1 : 0;
				break;
			case KEY_DOWN:
					pos += (pos < p.length - 1) ? 1 : 0;
				break;
		}
		for(int i = 0; i < p.length; i++) {
			if(i == pos) wattron(del_posts, A_BOLD);
			wprintw(del_posts, "[%s] – %s", ht_search(del_ht[i], "id"), ht_search(del_ht[i], "description"));
			if(i == pos) wattroff(del_posts, A_BOLD);
		}
		wrefresh(del_posts);
	} while((ch = wgetch(del_win)) != 27);

	//free memory
	wclear(del_posts);
	wclear(del_win);
	delwin(del_posts);
	delwin(del_win);
}

int
main()
{
	// initialize curses
	setlocale(LC_ALL, "");
	initscr();
	clear();
	cbreak();
	start_color();
	noecho();

	// get terminal size
	getmaxyx(stdscr, mrow, mcol);
	curs_set(0);

	// Header text
	headerView = newwin(1, mcol, 0, 0);
	wattron(headerView, A_REVERSE);
	wprintw(headerView, " welcome to suckless /g/ -- sucklessg.org \n");
	wattroff(headerView, A_REVERSE);
	wrefresh(headerView);

	// splash text
	WINDOW *splash = newwin(15, mcol, mrow / 4, mcol / 10);
	wprintw(splash, "███████╗██╗   ██╗ ██████╗██╗  ██╗██╗     ███████╗███████╗███████╗\n██╔════╝██║   ██║██╔════╝██║ ██╔╝██║     ██╔════╝██╔════╝██╔════╝\n███████╗██║   ██║██║     █████╔╝ ██║     █████╗  ███████╗███████╗\n╚════██║██║   ██║██║     ██╔═██╗ ██║     ██╔══╝  ╚════██║╚════██║\n███████║╚██████╔╝╚██████╗██║  ██╗███████╗███████╗███████║███████║\n╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝╚══════╝╚══════╝╚══════╝╚══════╝\n                                                                 \n                        ██╗ ██████╗     ██╗                      \n                       ██╔╝██╔════╝    ██╔╝                      \n                      ██╔╝ ██║  ███╗  ██╔╝                       \n                     ██╔╝  ██║   ██║ ██╔╝                        \n                    ██╔╝   ╚██████╔╝██╔╝                         \n                    ╚═╝     ╚═════╝ ╚═╝                          ");
	wrefresh(splash);
	delwin(splash);

	// makes a request to the server and returns the servers response.
	request_t request;
	request.r = malloc(BUFFER_SIZE);
	request.l = sprintf(request.r, "GET /page/0\r\nHost: %s\r\n\r\n", ADDRESS_URL);
	post_t threads = requestWrapper(request);
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

	// wait for exit key
	int ch;
	rowcount -= mrow-2;
	do {
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
					ht_length = (depth == 0) ? loadThread(ht_length, -1) : loadThread(ht_length, 0);
					free(thread_loc);
					thread_loc = malloc(sizeof(int) * ht_length);
				break;
			case 'R':
					//reply function
					displayReply(depth);
					ht_length = (depth == 0) ? loadThread(ht_length, -1) : loadThread(ht_length, 0);
					free(thread_loc);
					thread_loc = malloc(sizeof(int) * ht_length);
				break;
			case 'D':
					displayDelete();
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
			attr = (i == thread_pos) ? A_REVERSE | A_BOLD : A_REVERSE;
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
				wprintw(threadView, "pic: ");
				wattron(threadView, A_UNDERLINE);
				wprintw(threadView, "%s\n", ht_search(ht[i], "pic"));
				wattroff(threadView, A_UNDERLINE);
			}
			if (depth == 0) {
				wprintw(threadView, "(replies: %s)\n",  ht_search(ht[i], "replies"));
			}
			wprintw(threadView, "\n");
		}
		prefresh(threadView, thread_loc[thread_pos], 0, 1, 0, mrow-1, mcol);
	} while((ch = wgetch(threadView)) != 27);

	// remove window
	delwin(threadView);
	delwin(headerView);
	clear();
	refresh();
	endwin();

	// free memory
	free(thread_loc);
	for(int i = 0; i < ht_length; ++i)
		ht_del_hash_table(ht[i]);
	free(ht);
	for(int i = 0; i < threads.length; i++)
		free(threads.objects[i]);
	free(threads.objects);

	return (0);
}
