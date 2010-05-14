#File: Makefile.am
#Author: James Oakley
bin_PROGRAMS=katana


katana_CFLAGS=-I ../include -std=c99 -D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE  -I /usr/local/include/libdwarf -I /usr/include/libdwarf -Doff64_t=__off64_t

katana_LDFLAGS=-L /usr/local/lib
katana_LDADD= -ldwarf -lelf -lm -lunwind-generic -lunwind-ptrace

PATCHER_SRC=patcher/hotpatch.c patcher/target.c patcher/patchapply.c patcher/versioning.c patcher/linkmap.c patcher/safety.c patcher/pmap.c
PATCHWRITE_SRC=patchwrite/patchwrite.c patchwrite/codediff.c patchwrite/typediff.c patchwrite/elfwriter.c patchwrite/sourcetree.c patchwrite/write_to_dwarf.c patchwrite/elfcmp.c
UTIL_SRC=util/dictionary.c util/hash.c util/util.c util/map.c util/list.c util/logging.c util/path.c util/refcounted.c
INFO_SRC=info/fdedump.c info/dwinfo_dump.c  info/unsafe_funcs_dump.c

katana_SOURCES=katana.c dwarftypes.c   elfparse.c  types.c  dwarf_instr.c register.c relocation.c symbol.c fderead.c dwarfvm.c config.c $(PATCHWRITE_SRC) $(PATCHER_SRC) $(UTIL_SRC) $(INFO_SRC)

