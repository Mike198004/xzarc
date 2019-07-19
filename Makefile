#
# Author: Mike Soshnikov Copyright(C) 2019
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#

CC = cc
CFLAGS = -O2
LDFLAGS = -llzma -larchive

PROGS = \
	xzarc \

all: $(PROGS)

.c:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(PROGS)
