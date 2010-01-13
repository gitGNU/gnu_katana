CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-ldwarf -lelf

TYPEPATCH_SRC=typepatch.c dwarftypes.c dictionary.c hash.c target.c elfparse.c util.c

all :  patchee patched typepatch

patchee : main_v0.c
	$(CC) $(CFLAGS) -o patchee main_v0.c

patched : main_v1.c
	$(CC) $(CFLAGS) -o patched main_v1.c

typepatch : $(TYPEPATCH_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_TYPEPATCH)  -o typepatch $(TYPEPATCH_SRC) $(LDFLAGS_TYPEPATCH)

clean :
	rm patchee patched typepatch
	rm -f *.o *.s *.i