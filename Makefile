CC = gcc
CFLAGS = -Wall -std=c99

all: kilo

kilo: src/main.c
	$(CC) $(CFLAGS) $^ -o bin/$@

clean:
	rm -f bin/kilo
