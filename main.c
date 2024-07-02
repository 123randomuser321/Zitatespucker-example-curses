/*
	Zitatespucker-example-curses: Curses-based example frontend for Zitatespucker

    Copyright (C) 2024 by Sembo Sadur <labmailssadur@gmail.com>

	Permission to use, copy, modify, and/or distribute this software
	for any purpose with or without fee is hereby granted.

	THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
	WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
	IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
	OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
	NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
	TODO:
	KEY_UP, KEY_DOWN scroll through truncated quotes
	dynamic sizing
*/

#define LICENSE		"Copyright (C) 2024 by Sembo Sadur <labmailssadur@gmail.com>\n\n" \
					"Permission to use, copy, modify, and/or distribute this software\n" \
					"for any purpose with or without fee is hereby granted.\n\n" \
					"THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n" \
					"WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.\n" \
					"IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES\n" \
					"OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,\n" \
					"NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"



/* Standard headers */
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wctype.h>


/* pseudo-autodetection */
#ifdef __WIN32
	#ifndef USE_NCURSES
		#define USE_PDCURSES
	#endif
#else
	#ifndef USE_PDCURSES
		#define USE_NCURSES
	#endif
#endif


/* Curses */
#ifdef USE_NCURSES
	#define NCURSES_WIDECHAR 1
#endif
#ifdef USE_PDCURSES
    #define PDC_WIDE
#endif
#include <curses.h> // explicitely included
#include <panel.h>


/* Zitatespucker */
#define ZITATESPUCKER_JSON
#define ZITATESPUCKER_SQL
#include <Zitatespucker/Zitatespucker.h>


/* our filename is hardcoded, it's just a meme */
#define FILENAME_JSON "zitate.json"
#define FILENAME_SQL "zitate.sqlite"


/* Storage for wide strings */
struct zitatw {
	wchar_t *wauthor; // ZitatespuckerZitat->author as wide string
	wchar_t *wzitat; // ZitatespuckerZitat->zitat as wide string
	wchar_t *wcomment; // ZitatespuckerZitat->comment as wide string
};

/*
	macro to get the position at which to start printing, so that the output is centered
	X is the length of the line to print into, L is the line to be printed
*/
#define centerpos(X, L)		(((X) - (L) - (((X) - (L)) % 2)) / 2)

/*
	populate wstrings with the info from zitat
	if something bad happens, the respective pointer is NULL
*/
static void zitatwPopulate(struct zitatw *wstrings, ZitatespuckerZitat *zitat);

/*
	print author into win, third to last line, centered
*/
//static void wprintAuthor(wchar_t *author, WINDOW *win);

/*
	print author into win, third to last line, centered; including comment
*/
static void wprintAuthorComment(wchar_t *author, wchar_t *comment, WINDOW *win);

/*
	print zitat into win, centered
*/
static void wprintZitat(wchar_t *zitat, WINDOW *win);

/*
	print the date info into win (the valid parts), second to last line, centered
*/
static void wprintDate(bool annodomini, uint16_t year, uint8_t month, uint8_t day, WINDOW *win);

/*
	get the number of characters (including starting position) until, but not including, the next newline or null terminator
*/
//static size_t wcstillnew(wchar_t *wstr);

/*
	get the number of characters (including starting position) until, but not including, the next newline or null terminator

	if no newline or null terminator is encountered within lim characters,
	the function will return the position of the last blank space (iswblank()) within lim characters

	if that doesn't work, lim is returned
*/
static size_t wcslentilnewlim(wchar_t *wstr, size_t lim);

/*
	return a simple dumb random
*/
static int simpleDumbRandom(void);


int main(int argc, char **argv)
{
	/* 0. Make Unicode printable (specific locale required due to Windows, which displays garbled characters with empty locale) */
	setlocale(LC_ALL, "en_US.UTF-8");


	/* 0.5 command opts */
	bool useSQL = true;
	if (argc == 2) {
		if (strcmp(argv[1], "-j") == 0)
			useSQL = false;
		else if (strcmp(argv[1], "-v") == 0) {
			printf("Zitatespucker-example-curses\n\n%s", LICENSE);
			return 0;
		} else if (strcmp(argv[1], "-h") == 0) {
			printf("USAGE:\n%s [-j|-v|-h]\n\n-j: use \"%s\" instead of \"%s\"\n-v: show program info\n-h: show this message\n", argv[0], FILENAME_JSON, FILENAME_SQL);
			return 0;
		}
	}


	/* 1. Load File + contents */
	ZitatespuckerZitat *ZitatArrayStart = (useSQL ? ZitatespuckerSQLGetZitatAllFromFile(FILENAME_SQL) : ZitatespuckerJSONGetZitatAllFromFile(FILENAME_JSON));
	if (ZitatArrayStart == NULL) {
		fprintf(stderr, "\"%s\" not found :(\n", (useSQL ? FILENAME_SQL : FILENAME_JSON));
		return 1;
	}


	/* 2. initialize, run checks, setup (color, etc) */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, true);
	curs_set(0); // make cursor invisible
	if (!has_colors()) {
		// huh?
		printw("Your Terminal does not support colors!\n");
		getch(); // so you can actually see the message
		endwin();
		return 2;
	}
	start_color();

	init_pair(1, COLOR_WHITE, COLOR_BLUE); // stdscr, also its background
	//init_pair(2, COLOR_BLACK, COLOR_BLACK); // Zitat_bkgd, Legende_bkgd
	init_pair(3, COLOR_RED, COLOR_WHITE); // Zitat_frgd, Zitat_frgd_top
	init_pair(4, COLOR_WHITE, COLOR_RED); // Legende_frgd


	/* 3. define min/max sizes for windows */
	#define MINZITATX 60
	#define MINZITATY 20
	#define ZITATPADDINGX 4
	#define ZITATPADDINGY 2

	if (LINES < (MINZITATY + (ZITATPADDINGY * 2)) || COLS < (MINZITATX + (ZITATPADDINGX * 2))) {
		printw("Your teminal dimensions are insufficient.\nYou'll need at least %d lines and %d columns.\n(Found %d lines and %d columns)", MINZITATY + (ZITATPADDINGY * 2), MINZITATX + (ZITATPADDINGX * 2), LINES, COLS);
		getch();
		endwin();
		return 3;
	}
	

	/* 4. stdscr: set up background, print help indicator */
	wbkgd(stdscr, COLOR_PAIR(1));
	wattron(stdscr, COLOR_PAIR(1));
	mvwprintw(stdscr, LINES - 1, 0, "Press 'h' for help");
	wrefresh(stdscr);


	/* 5. Zitat_*: setup windows, associate with panels */
	int zitat_y = LINES - MINZITATY - (ZITATPADDINGY * 2);
	zitat_y = (zitat_y - (zitat_y % 2)) / 2;

	int zitat_x = COLS - MINZITATX - (ZITATPADDINGX * 2);
	zitat_x = (zitat_x - (zitat_x % 2)) / 2;

	WINDOW *Zitat_frgd = newwin(MINZITATY, MINZITATX, zitat_y + ZITATPADDINGY, zitat_x + ZITATPADDINGX);
	WINDOW *Zitat_frgd_top = newwin(MINZITATY - 5, MINZITATX - 4, zitat_y + ZITATPADDINGY + 1, zitat_x + ZITATPADDINGX + 2);
	// you may ask: why the seperate windows
	// this is due to (currently not implemented) scrolling
	// that way, you won't need to werase and refill Zitat_frgd, since the author et al does not change


	wbkgd(Zitat_frgd, COLOR_PAIR(3));
	wattron(Zitat_frgd, COLOR_PAIR(3));

	wbkgd(Zitat_frgd_top, COLOR_PAIR(3));
	wattron(Zitat_frgd_top, COLOR_PAIR(3));
	
	PANEL *Zitat_frgd_panel = new_panel(Zitat_frgd);
	PANEL *Zitat_frgd_top_panel = new_panel(Zitat_frgd_top);


	/* 6. Zitat_frgd: fill with first entry */
	struct zitatw wstrings;
	zitatwPopulate(&wstrings, ZitatArrayStart);
	//wprintAuthor(wstrings.wauthor, Zitat_frgd);
	wprintAuthorComment(wstrings.wauthor, wstrings.wcomment, Zitat_frgd);
	wprintZitat(wstrings.wzitat, Zitat_frgd_top);
	wprintDate(ZitatArrayStart->annodomini, ZitatArrayStart->year, ZitatArrayStart->month, ZitatArrayStart->day, Zitat_frgd);


	/* 7. Legende_*: setup windows, associate with panels */
	// Legende_frgd is 7y, 22x
	WINDOW *Legende_frgd = newwin(7, 22, ((LINES - 7) - ((LINES - 7) % 2)) / 2, ((COLS - 22) - ((COLS - 22) % 2)) / 2);

	wbkgd(Legende_frgd, COLOR_PAIR(4));
	wattron(Legende_frgd, COLOR_PAIR(4) | A_BOLD);
	PANEL *Legende_frgd_panel = new_panel(Legende_frgd);
	
	#define HELP_LEFT	" --> previous entry"
	#define HELP_RIGHT	" --> next entry"
	#define HELP_R		"r --> random entry"
	#define HELP_H		"h --> exit help"
	#define HELP_Q		"q --> Quit"
	// longest one is HELP_LEFT, with 19 + 1 printable characters

	mvwaddch(Legende_frgd, 1, 1, ACS_LARROW);
	mvwprintw(Legende_frgd, 1, 2, "%s", HELP_LEFT);
	mvwaddch(Legende_frgd, 2, 1, ACS_RARROW);
	mvwprintw(Legende_frgd, 2, 2, "%s", HELP_RIGHT);
	mvwprintw(Legende_frgd, 3, 1, "%s", HELP_R);
	mvwprintw(Legende_frgd, 4, 1, "%s", HELP_H);
	mvwprintw(Legende_frgd, 5, 1, "%s", HELP_Q);


	/* 8. hide Legende_*, update_panels(), doupdate() */
	hide_panel(Legende_frgd_panel);
	bool Legende_frgd_hidden = true; // panel_hidden() does not seem to work on PDCurses
	update_panels();
	doupdate();


	/* 9. Enter get_wch()-loop */
	ZitatespuckerZitat *curZitat = ZitatArrayStart;
	wint_t wch;
	int loopcon;
	bool shouldexit = false;
	size_t ListLen = ZitatespuckerZitatListLen(ZitatArrayStart);
	int ran;

	while (shouldexit == false && ((loopcon = get_wch(&wch)) == KEY_CODE_YES || loopcon == OK)) {
		if (Legende_frgd_hidden) {
			switch(wch) {
				case KEY_LEFT:
				if (curZitat->prevZitat != NULL) {
					curZitat = curZitat->prevZitat;
					zitatwPopulate(&wstrings, curZitat);
					werase(Zitat_frgd);
					werase(Zitat_frgd_top);
					//wprintAuthor(wstrings.wauthor, Zitat_frgd);
					wprintAuthorComment(wstrings.wauthor, wstrings.wcomment, Zitat_frgd);
					wprintZitat(wstrings.wzitat, Zitat_frgd_top);
					wprintDate(curZitat->annodomini, curZitat->year, curZitat->month, curZitat->day, Zitat_frgd);
					update_panels();
					doupdate();
				} else
					flash();
				break;

				case KEY_RIGHT:
				if (curZitat->nextZitat != NULL) {
					curZitat = curZitat->nextZitat;
					zitatwPopulate(&wstrings, curZitat);
					werase(Zitat_frgd);
					werase(Zitat_frgd_top);
					//wprintAuthor(wstrings.wauthor, Zitat_frgd);
					wprintAuthorComment(wstrings.wauthor, wstrings.wcomment, Zitat_frgd);
					wprintZitat(wstrings.wzitat, Zitat_frgd_top);
					wprintDate(curZitat->annodomini, curZitat->year, curZitat->month, curZitat->day, Zitat_frgd);
					update_panels();
					doupdate();
				} else
					flash();
				break;

				case L'r':
				case L'R':
				ran = simpleDumbRandom();
				ran = ran % ListLen; // theoretically, this can be bonkers
				curZitat = ZitatArrayStart;
				for (int i = 0; i < ran; i++)
					curZitat = curZitat->nextZitat;
				zitatwPopulate(&wstrings, curZitat);
				werase(Zitat_frgd);
				werase(Zitat_frgd_top);
				//wprintAuthor(wstrings.wauthor, Zitat_frgd);
				wprintAuthorComment(wstrings.wauthor, wstrings.wcomment, Zitat_frgd);
				wprintZitat(wstrings.wzitat, Zitat_frgd_top);
				wprintDate(curZitat->annodomini, curZitat->year, curZitat->month, curZitat->day, Zitat_frgd);
				update_panels();
				doupdate();
				break;

				case L'h':
				case L'H':
				show_panel(Legende_frgd_panel);
				Legende_frgd_hidden = false;
				update_panels();
				doupdate();
				break;

				case L'q':
				case L'Q':
				shouldexit = true;
				break;
			}
		} else {
			switch(wch) {
				case L'h':
				case L'H':
				hide_panel(Legende_frgd_panel);
				Legende_frgd_hidden = true;
				update_panels();
				doupdate();
				break;

				case L'q':
				case L'Q':
				shouldexit = true;
				break;
			}
		}
	}


	/* 10. del_panel(), delwin(), endwin() */

	if (wstrings.wauthor != NULL)
		free((void *) wstrings.wauthor);
	if (wstrings.wzitat != NULL)
		free((void *) wstrings.wzitat);

	del_panel(Legende_frgd_panel);
	delwin(Legende_frgd);
	del_panel(Zitat_frgd_top_panel);
	delwin(Zitat_frgd_top);
	del_panel(Zitat_frgd_panel);
	delwin(Zitat_frgd);
	endwin();
	ZitatespuckerZitatFree(ZitatArrayStart);

	return 0;
}


static void zitatwPopulate(struct zitatw *wstrings, ZitatespuckerZitat *zitat)
{
	if (wstrings == NULL || zitat == NULL)
		return;
	
	// reset entries
	wstrings->wauthor = NULL;
	wstrings->wzitat = NULL;
	wstrings->wcomment = NULL;
	
	// author
	if (zitat->author != NULL) {
		size_t authorlen = (mbstowcs(NULL, zitat->author, 0) + 1);
		wchar_t *wauthor = (wchar_t *) malloc(authorlen * sizeof(wchar_t));

		if (wauthor != NULL)
			mbstowcs(wauthor, zitat->author, authorlen);
		
		wstrings->wauthor = wauthor;
	}

	// zitat
	if (zitat->zitat != NULL) {
		size_t zitatlen = (mbstowcs(NULL, zitat->zitat, 0) + 1);
		wchar_t *wzitat = (wchar_t *) malloc(zitatlen * sizeof(wchar_t));

		if (wzitat != NULL)
			mbstowcs(wzitat, zitat->zitat, zitatlen);
		
		wstrings->wzitat = wzitat;
	}

	// comment
	if (zitat->comment != NULL) {
		size_t commentlen = (mbstowcs(NULL, zitat->comment, 0) + 1);
		wchar_t *wcomment = (wchar_t *) malloc(commentlen * sizeof(wchar_t));

		if (wcomment != NULL)
			mbstowcs(wcomment, zitat->comment, commentlen);
		
		wstrings->wcomment = wcomment;
	}

	return;
}


/*
static void wprintAuthor(wchar_t *author, WINDOW *win)
{
	if (author == NULL || win == NULL)
		return;
	
	int zitat_y_max, zitat_x_max;
	getmaxyx(win, zitat_y_max, zitat_x_max);
	zitat_y_max--;
	zitat_x_max--;

	// print
	mvwaddwstr(win, zitat_y_max - 2, centerpos(zitat_x_max, wcslen(author)), author);
}
*/


static void wprintAuthorComment(wchar_t *author, wchar_t *comment, WINDOW *win)
{
	if (author == NULL || win == NULL)
		return;
	
	int zitat_y_max, zitat_x_max;
	getmaxyx(win, zitat_y_max, zitat_x_max);
	zitat_y_max--;
	zitat_x_max--;

	size_t len = wcslen(author);

	if (comment != NULL) {
		len += (wcslen(comment) + 2);
	}

	// print
	mvwaddwstr(win, zitat_y_max - 2, centerpos(zitat_x_max, len), author);
	if (comment != NULL) {
		waddwstr(win, L", ");
		waddwstr(win, comment);
	}
}


static void wprintDate(bool annodomini, uint16_t year, uint8_t month, uint8_t day, WINDOW *win)
{
	if (annodomini == false && year == 0)
		return;
	
	int zitat_y_max, zitat_x_max;
	getmaxyx(win, zitat_y_max, zitat_x_max);
	zitat_y_max--;
	zitat_x_max--;
	
	size_t datelen = 1;
	uint16_t tmp = year;
	while ((tmp /= 10) > 0)
		datelen++;
	
	// this somehow looks dumb
	if (month != 0) {
		datelen += 3;
		if (month > 12)
			month = 12;

		if (day != 0) {
			datelen += 3;
			if (day > 31)
				day = 31;
		}
	}

	mvwprintw(win, zitat_y_max - 1, centerpos(zitat_x_max, datelen), "%d", year);
	if (month != 0) {
		wprintw(win, ".%s%d", month < 10 ? "0" : "", month);

		if (day != 0)
			wprintw(win, ".%s%d", day < 10 ? "0" : "", day);
	}

	return;
}


static void wprintZitat(wchar_t *zitat, WINDOW *win)
{
	if (zitat == NULL || win == NULL)
		return;
	
	int zitat_x_max = getmaxx(win);
	zitat_x_max--;
	
	wchar_t *tmp = zitat;
	int tillnew = (int) wcslentilnewlim(tmp, zitat_x_max);

	for (int i = 0; *tmp != L'\0' && i < MINZITATY - 6; i++) {
		mvwaddnwstr(win, i, (centerpos(zitat_x_max, tillnew)), tmp, tillnew);
		tmp += tillnew;
		if (*tmp == L'\0')
			break;
		else {
			tmp++;
			tillnew = (int) wcslentilnewlim(tmp, zitat_x_max);
		}
	}
}


/*
static size_t wcstillnew(wchar_t *wstr)
{
	size_t ret = 0;

	while (*wstr != L'\n' && *wstr != L'\0') {
		ret++;
		wstr++;
	}
	
	return ret;
}
*/


static size_t wcslentilnewlim(wchar_t *wstr, size_t lim)
{
	size_t ret = 0;

	while (*wstr != L'\n' && *wstr != L'\0' && ret < lim) {
		wstr++;
		ret++;
	}

	if (*wstr != L'\n' && *wstr != L'\0') {
		while (!iswblank(*wstr) && ret > 0) {
			wstr--;
			ret--;
		}
		if (ret == 0)
			ret = lim;
	}

	return ret;
}


static int simpleDumbRandom(void)
{
	static unsigned int seed;

	seed += (unsigned int) time(NULL); // this may overflow
	srand(seed);
	int ret = rand();
	seed += ret;

	return ret;
}
