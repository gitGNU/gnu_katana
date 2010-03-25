MCXX=g++
CC=gcc
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE -I ../src/

SRC=listsort.c ../src/util/list.c

listsort : $(SRC)
	$(CC) $(CFLAGS) -o listsort $(SRC)
