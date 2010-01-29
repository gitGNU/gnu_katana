CXX=g++
CC=gcc
#todo: remove 32-bit dependency
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32 -I src/
CFLAGS_TYPEPATCH=-Doff64_t=__off64_t
LDFLAGS_TYPEPATCH=-L /usr/local/lib -ldwarf -lelf -lm

TYPEPATCH_SRC=src/katana.c src/dwarftypes.c src/dictionary.c src/hash.c src/patcher/target.c src/elfparse.c src/util.c src/types.c src/map.c src/patcher/hotpatch.c src/patchwrite/patchwrite.c src/dwarf_instr.c

EXEC=katana

all :  $(EXEC)

check : all
	make -C tests
	./run_tests.py

$(EXEC) : $(TYPEPATCH_SRC)
	$(CC) $(CFLAGS) $(CFLAGS_TYPEPATCH)  -o $(EXEC) $(TYPEPATCH_SRC) $(LDFLAGS_TYPEPATCH)



clean :
	rm -f $(EXEC)
	rm -f *.o *.s *.i
	make -C tests clean

#don't you love sed? It does so many marvelous things, but looking at
#this line you haven't the foggiest clue what it might do.
#What it does do is extract the last component of the path name
#so that things in the tar archive can be named appropriately
DIR_TO_TAR=$(shell pwd | sed 's/\(.*\/\)\(.*\)/\2/')
dist : all
	cd ../; tar -czf katana.tar.gz --exclude-from=$(PWD)/tar-excludes $(DIR_TO_TAR)