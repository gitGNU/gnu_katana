
CXX=g++
CC=gcc
CFLAGS_RELOC=-Xlinker --emit-relocs
#todo: remove 32-bit dependency
CFLAGS=-Wall -g $(CFLAGS_RELOC) -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -m32
EXEC=test
export CXX
export CC
export CFLAGS
export EXEC

SUBDIRS=test1_basic _test3_multiple_cu
.PHONY: $(SUBDIRS)

all : $(SUBDIRS)

$(SUBDIRS) :
	make -C $@ all


clean :
	-for d in $(SUBDIRS); do (cd $$d; make clean ); done