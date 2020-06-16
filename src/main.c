/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#include "editor.h"
#include "output.h"

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editor_config config;

/*** terminal ***/

void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	perror(s);
	exit(1);
}

void disable_raw_mode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &config.original_terminal) == -1)
		die("tcsetattr with disabling raw mode");
}

void enable_raw_mode() {
	if (tcgetattr(STDIN_FILENO, &config.original_terminal) == -1)
		die("tcgetattr");

	struct termios raw = config.original_terminal;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

int editor_read_key() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN)
			die("read");
	}

	if (c == '\x1b') {
		char seq[3];

		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME;
						case '3': return DELETE;
						case '4': return END;
						case '5': return PAGE_UP;
						case '6': return PAGE_DOWN;
						case '7': return HOME;
						case '8': return END;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME;
					case 'F': return END;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME;
				case 'F': return END;
			}
		}
	}
	return c;
}

int get_cursor_position(int *rows, int *cols) {
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
		return -1;
	
	char buffer[32];
	unsigned int i = 0;
	
	while (i < sizeof(buffer) - 1) {
		if (read(STDIN_FILENO, &buffer[i], 1) != 1)
			break;
		if (buffer[i] == 'R')
			break;
		++i;
	}
	buffer[i] = '\0';

	if (buffer[0] != '\x1b' || buffer[1] != '[')
		return -1;
	if (sscanf(&buffer[2], "%d;%d", rows, cols) != 2)
		return -1;

	return 0;
}

int get_window_size(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		// position cursor at bottom right of screen if ioctl fails
		// this is a fallback method for getting the size of the window
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
			return -1;
		return get_cursor_position(rows, cols);
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		return -1;
	 
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
}

/*** row operations ***/

void editor_append_row(char *s, size_t len) {
	config.row = realloc(config.row, sizeof(erow) * (config.numrows + 1));

	int at = config.numrows;
	config.row[at].size = len;
	config.row[at].chars = malloc(len + 1);
	memcpy(config.row[at].chars, s, len);
	config.row[at].chars[len] = '\0';
	++config.numrows;
}

/*** file i/o ***/

void editor_open(char *filename) {
	FILE *fp = fopen(filename, "r");
	if (!fp) die("fopen");

	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	while ((linelen = getline(&line, &linecap, fp)) != -1) {
		while (linelen > 0 && (line[linelen-1] == '\n' || line[linelen-1] == '\r'))
			--linelen;
		editor_append_row(line, linelen);
	}
	free(line);
	fclose(fp);
}

/*** input ***/

void editor_move_cursor(int key) {
	switch (key) {
		case ARROW_LEFT:
			if (config.cx != 0)
				--config.cx;
			break;
		case ARROW_RIGHT:
			if (config.cx != config.screencols - 1)
				++config.cx;
			break;
		case ARROW_UP:
			if (config.cy != 0)
				--config.cy;
			break;
		case ARROW_DOWN:
			if (config.cy != config.screenrows - 1)
				++config.cy;
			break;
	}
}

void editor_process_keypress() {
	int c = editor_read_key();

	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;

		case HOME:
			config.cx = 0;
			break;
		case END:
			config.cx = config.screencols - 1;
			break;

		case PAGE_UP:
		case PAGE_DOWN:
			{
				int times = config.screenrows;
				while (--times)
					editor_move_cursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
			}
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_RIGHT:
		case ARROW_LEFT:
			editor_move_cursor(c);
			break;
	}
}

/*** init ***/

void init_editor() {
	config.cx = 0;
	config.cy = 0;
	config.numrows = 0;
	config.row = NULL;

	if (get_window_size(&config.screenrows, &config.screencols) == -1)
		die("get_window_size");
}

int main(int argc, char *argv[]) {
	enable_raw_mode();
	atexit(disable_raw_mode);
	init_editor();
	if (argc >= 2) 
		editor_open(argv[1]);

	while (1) {
		editor_refresh_screen(&config);
		editor_process_keypress();
	}
	return 0;
}
