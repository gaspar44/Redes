
CFLAGS=-std=c99 -g3 -O0 -Wall -pedantic -Wextra -Wshadow -Wpointer-arith \
	-Wcast-qual -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wconversion -Wno-overlength-strings \
	-D_POSIX_SOURCE=1 -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64

.PHONY: all clean

all:
	$(CC) $(CFLAGS) src/pong.c -o bin/pong
	
	$(CC) $(CFLAGS) src/ttorrent.c src/file_io.c src/logger.c -o bin/ttorrent -lssl -lcrypto

clean:
	rm -f bin/pong bin/ttorrent
