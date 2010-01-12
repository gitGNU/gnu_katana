CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-lelf

all :  patchee patched typepatch

patchee : main_v0.c
	$(CC) $(CFLAGS) -o patchee main_v0.c

patched : main_v1.c
	$(CC) $(CFLAGS) -o patched main_v1.c

typepatch : typepatch.c
	$(CC) $(CFLAGS) $(CFLAGS_TYPEPATCH) $(LDFLAGS_TYPEPATCH) -o typepatch typepatch.c

clean :
	rm patchee patched typepatch
	rm -f *.o *.s *.i