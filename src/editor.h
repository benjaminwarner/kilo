#ifndef EDITOR_H
#define EDITOR_H

#include <termios.h>

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

typedef struct erow {
	int size;
	int rsize;
	char *chars;
	char *render;
} erow;

struct editor_config {
	int cx, cy;
	int row_offset;
	int col_offset;
	int screenrows;
	int screencols;
	int numrows;
	erow *row;
	struct termios original_terminal;
};

#endif
