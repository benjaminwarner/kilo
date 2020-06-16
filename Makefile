CC = gcc
CFLAGS = -Wall -std=c99

all: kilo

kilo: src/editor.h src/output.h src/output.c src/version.h src/main.c
	$(CC) $(CFLAGS) $^ -o bin/$@

clean:
	rm -f bin/kilo
