/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 1
#define CTRL_KEY(k) ((k) & 0x1f)

enum editor_key {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	DELETE,
	HOME,
	END,
	PAGE_UP,
	PAGE_DOWN
};

/*** data ***/

struct editor_config {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios original_terminal;
};

struct editor_config config;

/*** function defs ***/

void editor_refresh_screen();

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

/*** append buffer ***/

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
		return;
	
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void abFree(struct abuf *ab) {
	free(ab->b);
}

/*** output ***/

void editor_draw_rows(struct abuf *ab) {
	for (int y = 0; y < config.screenrows; y++) {
		if (y == config.screenrows / 3) {
			char welcome[80];
			int welcomelen = snprintf(welcome, sizeof(welcome),
				"Kilo Editor -- version %d.%d.%d",
				VERSION_MAJOR,
				VERSION_MINOR,
				VERSION_PATCH
			);
			if (welcomelen > config.screencols) welcomelen = config.screencols;
			int padding = (config.screencols - welcomelen) / 2;
			if (padding) {
				abAppend(ab, "~", 1);
				padding--;
			}
			while (padding--) abAppend(ab, " ", 1);
			abAppend(ab, welcome, welcomelen);
		} else {
			abAppend(ab, "~", 1);
		}

		abAppend(ab, "\x1b[K", 3);
		if (y < config.screenrows - 1)
			abAppend(ab, "\r\n", 2);
	}
}

void editor_refresh_screen() {
	struct abuf ab = ABUF_INIT;

	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);

	editor_draw_rows(&ab);

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", config.cy + 1, config.cx + 1);
	abAppend(&ab, buffer, strlen(buffer));

	abAppend(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
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

	if (get_window_size(&config.screenrows, &config.screencols) == -1)
		die("get_window_size");
}

int main(int argc, char *argv[]) {
	enable_raw_mode();
	atexit(disable_raw_mode);
	init_editor();

	while (1) {
		editor_refresh_screen();
		editor_process_keypress();
	}
	return 0;
}
