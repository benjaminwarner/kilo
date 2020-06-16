#ifndef EDITOR_OUTPUT_H
#define EDITOR_OUTPUT_H

#include "editor.h"

struct abuf {
	char *b;
	int len;
};

#define ABUF_INIT {NULL, 0}

void ab_append(struct abuf *ab, const char *s, int len);
void ab_free(struct abuf *ab);

void editor_draw_rows(struct abuf *ab, struct editor_config *config);
void editor_draw_version_row(struct abuf *ab, struct editor_config *config);
void editor_refresh_screen(struct editor_config *config);

#endif
