#
# Author: Genie (C) 2019
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#

CC = gcc
CFLAGS = -O2 -g
LDFLAGS = -llzma -larchive

PROGS = \
	xzarc \

all: $(PROGS)

.c:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(PROGS)
