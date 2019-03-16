#include <ncurses.h>
#include "sucklessg.c"

WINDOW *pad;
WINDOW *w;
static int mrow, mcol;

int main(int argc, char **argv)
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
	wprintw(w, "welcome to suckless \n");
	wattroff(w, A_REVERSE);
	wrefresh(w);

	// makes a request to the server and returns the servers response.
	post_t threads = makeRequest("GET", "/page/0");

	// load the hash table

	int rowcount = threads.length * 5;

	// create pad
	pad = newpad (rowcount+2, mcol);
	keypad(pad, TRUE);
	// col titles
	wattron(pad, A_UNDERLINE);
	wprintw(pad, "press UP/DOWN to view.\n");
	wattroff(pad, A_UNDERLINE);
	wrefresh(pad);

	for (int i = 0; i < threads.length; i++) {
		wprintw(pad, "%s\n\n", threads.objects[i]); 
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
	for(int i = 0; i < threads.length; i++)
		free(threads.objects[i]);
	free(threads.objects);

	return (0);
}
