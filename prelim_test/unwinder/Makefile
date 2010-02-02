CXX=g++
CC=gcc
#todo: remove 32-bit dependency
CFLAGS=-Wall -g -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32 -Doff64_t=__off64_t -I ../../katana/src
LDFLAGS=-L /usr/local/lib -ldwarf -lelf -lm

UNWINDER_SRC=unwind.c unwind_dwarf_helper.c ../../katana/src/patcher/hotpatch.c ../../katana/src/elfparse.c ../../katana/src/types.c ../../katana/src/util/util.c ../../katana/src/util/dictionary.c ../../katana/src/util/map.c ../../katana/src/patcher/target.c ../../katana/src/util/hash.c ../../katana/src/dwarf_instr.c ../../katana/src/register.c

all : unwinder testprog

unwinder : $(UNWINDER_SRC)
	$(CC) $(CFLAGS) -o unwinder $(UNWINDER_SRC) $(LDFLAGS)

testprog : testprog.c
	$(CC) $(CFLAGS) -o testprog testprog.c

clean :
	rm -f *.o unwinder testprog