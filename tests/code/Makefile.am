bin_PROGRAMS=listsort lebtest
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE -I ../../src/

listsort_SOURCES=listsort.c ../../src/util/list.c

lebtest_SOURCES=lebtest.c ../../src/leb.c ../../src/util/util.c
lebtest_LDFLAGS=-lm