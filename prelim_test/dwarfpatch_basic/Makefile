CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
#todo: remove 32-bit dependency
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-L /usr/local/lib -ldwarf -lelf

TYPEPATCH_SRC=typepatch.c dwarftypes.c dictionary.c hash.c target.c elfparse.c util.c types.c map.c hotpatch.c

all :  patchee patched typepatch

patchee : main_v0.c
	$(CC) $(CFLAGS) -o patchee main_v0.c

patched : main_v1.c
	$(CC) $(CFLAGS) -o patched main_v1.c

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
	cd ../; tar -czf typepatch_dwarf.tar.gz --exclude-from=$(PWD)/tar-excludes $(DIR_TO_TAR)