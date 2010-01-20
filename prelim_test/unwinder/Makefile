CXX=g++
CC=gcc
#todo: remove 32-bit dependency
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32 -Doff64_t=__off64_t -I ../../katana/src
LDFLAGS=-L /usr/local/lib -ldwarf -lelf

UNWINDER_SRC=unwind.c unwind_dwarf_helper.c ../../katana/src/hotpatch.c ../../katana/src/elfparse.c ../../katana/src/types.c ../../katana/src/util.c ../../katana/src/dictionary.c ../../katana/src/map.c ../../katana/src/target.c ../../katana/src/hash.c

all : unwinder testprog

unwinder : $(UNWINDER_SRC)
	$(CC) $(CFLAGS) -o unwinder $(UNWINDER_SRC) $(LDFLAGS)

testprog : testprog.c
	$(CC) $(CFLAGS) -o testprog testprog.c

clean :
	rm -f *.o unwinder testprog