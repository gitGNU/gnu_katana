CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
#todo: remove 32-bit dependency
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32

SRC=main.c
EXEC=test

all : $(EXEC)

test : $(SRC)
	$(CC) $(CFLAGS) -o $(EXEC) $(SRC)