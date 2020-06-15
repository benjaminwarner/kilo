/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// this is following a text editor learning project
// which can be found here: https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

/*** data ***/

struct termios original_terminal;

/*** terminal ***/

void die(const char *s) {
	perror(s);
	exit(1);
}

void disable_raw_mode() {
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_terminal) == -1)
		die("tcsetattr with disabling raw mode");
}

void enable_raw_mode() {
	if (tcgetattr(STDIN_FILENO, &original_terminal) == -1)
		die("tcgetattr");

	struct termios raw = original_terminal;
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	raw.c_oflag &= ~(OPOST);
	raw.c_cflag |= (CS8);
	raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	raw.c_cc[VMIN] = 0;
	raw.c_cc[VTIME] = 1;

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
		die("tcsetattr");
}

/*** init ***/

int main(int argc, char *argv[]) {
	enable_raw_mode();
	atexit(disable_raw_mode);

	while (1) {
		char c = '\0';
		if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN)
			die("read");
		if (iscntrl(c))
			printf("%d\r\n", c);
		else
			printf("%d ('%c')\r\n", c, c);
		if (c == 'q')
			break;
	}
	return 0;
}
