# SPDX-License-Identifier: GPL-3.0-only

CFLAGS ?= -Wall -Wextra -pedantic

pjfs-fuse: main.o pjfs.o
	$(CC) -lfuse3 -o $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f pjfs-fuse *.o
