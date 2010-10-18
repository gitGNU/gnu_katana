#File: Makefile.am
#Author: James Oakley
bin_PROGRAMS=katana


katana_CFLAGS=-I ../include -g -Wall -Werror -Wno-error=pointer-sign -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE  -I /usr/local/include/libdwarf -I /usr/include/libdwarf -Doff64_t=__off64_t -DDEBUG

katana_LDFLAGS=-L /usr/local/lib -L ../external/
katana_LDADD= -ldwarf -lelf -lm -lunwind-generic -lunwind-ptrace

PATCHER_SRC=patcher/hotpatch.c patcher/target.c patcher/patchapply.c patcher/versioning.c patcher/linkmap.c patcher/safety.c patcher/pmap.c
PATCHER_H=patcher/hotpatch.h patcher/target.h patcher/patchapply.h patcher/versioning.h patcher/linkmap.h patcher/safety.h patcher/pmap.h
PATCHWRITE_SRC=patchwrite/patchwrite.c patchwrite/codediff.c patchwrite/typediff.c patchwrite/elfwriter.c patchwrite/sourcetree.c patchwrite/write_to_dwarf.c patchwrite/elfcmp.c
PATCHWRITE_H=patchwrite/patchwrite.h patchwrite/codediff.h patchwrite/typediff.h patchwrite/elfwriter.h patchwrite/sourcetree.h patchwrite/write_to_dwarf.h patchwrite/elfcmp.h
UTIL_SRC=util/dictionary.c util/hash.c util/util.c util/map.c util/list.c util/logging.c util/path.c util/refcounted.c
UTIL_H=util/dictionary.h util/hash.h util/util.h util/map.h util/list.h util/logging.h util/path.h util/refcounted.h
INFO_SRC=info/fdedump.c info/dwinfo_dump.c  info/unsafe_funcs_dump.c
INFO_H=info/fdedump.h info/dwinfo_dump.h  info/unsafe_funcs_dump.h

H_FILES=dwarftypes.h elfparse.h types.h dwarf_instr.h register.h relocation.h symbol.h fderead.h dwarfvm.h config.h arch.h constants.h leg.h $(PATCHER_H) $(PATCHWRITE_H) $(UTIL_H) $(INFO_H)

EXTRA_DIST=$(H_FILES)

katana_SOURCES=katana.c dwarftypes.c   elfparse.c  types.c  dwarf_instr.c register.c relocation.c symbol.c fderead.c dwarfvm.c config.c leb.c $(PATCHWRITE_SRC) $(PATCHER_SRC) $(UTIL_SRC) $(INFO_SRC)

