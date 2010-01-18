CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
#todo: remove 32-bit dependency
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-L /usr/local/lib -ldwarf -lelf

TYPEPATCH_SRC=src/typepatch.c src/dwarftypes.c src/dictionary.c src/hash.c src/target.c src/elfparse.c src/util.c src/types.c src/map.c src/hotpatch.c

all :  patchee patched typepatch

patchee : tests/main_v0.c
	$(CC) $(CFLAGS) -o patchee tests/main_v0.c

patched : tests/main_v1.c
	$(CC) $(CFLAGS) -o patched tests/main_v1.c

typepatch : $(TYPEPATCH_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_TYPEPATCH)  -o typepatch $(TYPEPATCH_SRC) $(LDFLAGS_TYPEPATCH)

clean :
	rm -f patchee patched typepatch
	rm -f *.o *.s *.i

#don't you love sed? It does so many marvelous things, but looking at
#this line you haven't the foggiest clue what it might do.
#What it does do is extract the last component of the path name
#so that things in the tar archive can be named appropriately
DIR_TO_TAR=$(shell pwd | sed 's/\(.*\/\)\(.*\)/\2/')
dist : all
	cd ../; tar -czf katana --exclude-from=$(PWD)/tar-excludes $(DIR_TO_TAR)