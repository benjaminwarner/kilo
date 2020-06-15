/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editor_config {
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

char editor_read_key() {
	int nread;
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN)
			die("read");
	}
	return c;
}

int get_window_size(int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		return -1;
	 
	*cols = ws.ws_col;
	*rows = ws.ws_row;
	return 0;
}

/*** output ***/

void editor_draw_rows() {
	for (int y = 0; y < config.screenrows; y++) {
		write(STDOUT_FILENO, "~\r\n", 3);
	}
}

void editor_refresh_screen() {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);

	editor_draw_rows();

	write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

void editor_process_keypress() {
	char c = editor_read_key();

	switch (c) {
		case CTRL_KEY('q'):
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit(0);
			break;
	}
}

/*** init ***/

void init_editor() {
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
