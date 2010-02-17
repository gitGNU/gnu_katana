CXX=g++
CC=gcc
#todo: remove 32-bit dependency
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE -m32 -I ../src/

SRC=listsort.c ../src/util/list.c

listsort : $(SRC)
	$(CC) $(CFLAGS) -o listsort $(SRC)
