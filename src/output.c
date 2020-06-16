#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "editor.h"
#include "output.h"
#include "version.h"

void ab_append(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);

	if (new == NULL)
		return;
	
	memcpy(&new[ab->len], s, len);
	ab->b = new;
	ab->len += len;
}

void ab_free(struct abuf *ab) {
	free(ab->b);
}

void editor_draw_rows(struct abuf *ab, struct editor_config *config) {
	int version_line_row = config->screenrows / 3;
	for (int y = 0; y < config->screenrows; ++y) {
		if (y < config->numrows) {
			int len = config->row.size;
			if (len > config->screencols)
				len = config->screencols;
			ab_append(ab, config->row.chars, len);
		} else if (y == version_line_row && config->numrows == 0) {
			editor_draw_version_row(ab, config);
		} else {
			ab_append(ab, "~", 1);
		}

		ab_append(ab, "\x1b[K", 3);
		if (y < config->screenrows - 1)
			ab_append(ab, "\r\n", 2);
	}
}

void editor_draw_version_row(struct abuf *ab, struct editor_config *config) {
	char editor_version[80];
	int len = snprintf(editor_version, sizeof(editor_version),
		"Kilo Editor -- version %d.%d.%d",
		VERSION_MAJOR,
		VERSION_MINOR,
		VERSION_PATCH
	);
	if (len > config->screencols) len = config->screencols;
	int padding = (config->screencols - len) / 2;
	if (padding) {
		ab_append(ab, "~", 1);
		--padding;
	}
	while (--padding) ab_append(ab, " ", 1);
	ab_append(ab, editor_version, len);
}

void editor_refresh_screen(struct editor_config *config) {
	struct abuf ab = ABUF_INIT;

	ab_append(&ab, "\x1b[?25l", 6);
	ab_append(&ab, "\x1b[H", 3);

	editor_draw_rows(&ab, config);

	char buffer[32];
	snprintf(buffer, sizeof(buffer), "\x1b[%d;%dH", config->cy + 1, config->cx + 1);
	ab_append(&ab, buffer, strlen(buffer));

	ab_append(&ab, "\x1b[?25h", 6);

	write(STDOUT_FILENO, ab.b, ab.len);
	ab_free(&ab);
}
