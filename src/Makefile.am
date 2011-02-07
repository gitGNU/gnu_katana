#File: Makefile.am
#Author: James Oakley
SUBDIRS=shell
bin_PROGRAMS=katana

INCLUDEFLAGS=-I ../include -I /usr/local/include/libdwarf -I /usr/include/libdwarf
DEFINEFLAGS=-D_POSIX_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE -D_GNU_SOURCE -DDEBUG -Doff64_t=__off64_t 

katana_CFLAGS=$(INCLUDEFLAGS) -g -Wall -std=c99 $(DEFINEFLAGS)

katana_CPPFLAGS=$(INCLUDEFLAGS) -g -Wall  $(DEFINEFLAGS)


katana_LDFLAGS=-L /usr/local/lib -L ../external/
katana_LDADD= -ldwarf -lelf -lm -lunwind-generic -lunwind-ptrace -lreadline

PATCHER_SRC=patcher/hotpatch.c patcher/target.c patcher/patchapply.c patcher/versioning.c patcher/linkmap.c patcher/safety.c patcher/pmap.c
PATCHER_H=patcher/hotpatch.h patcher/target.h patcher/patchapply.h patcher/versioning.h patcher/linkmap.h patcher/safety.h patcher/pmap.h
PATCHWRITE_SRC=patchwrite/patchwrite.c patchwrite/codediff.c patchwrite/typediff.c  patchwrite/sourcetree.c patchwrite/write_to_dwarf.c patchwrite/elfcmp.c
PATCHWRITE_H=patchwrite/patchwrite.h patchwrite/codediff.h patchwrite/typediff.h patchwrite/sourcetree.h patchwrite/write_to_dwarf.h patchwrite/elfcmp.h
UTIL_SRC=util/dictionary.c util/hash.c util/util.c util/map.c util/list.c util/logging.c util/path.c util/refcounted.c util/stack.c util/cxxutil.cpp util/growingBuffer.c util/file.c
UTIL_H=util/dictionary.h util/hash.h util/util.h util/map.h util/list.h util/logging.h util/path.h util/refcounted.h util/stack.h util/cxxutil.h util/growingBuffer.h util/file.h
SHELL_VARIABLE_SRC=shell/variableTypes/elfVariableData.cpp shell/variableTypes/rawVariableData.cpp shell/variableTypes/arrayData.cpp shell/variableTypes/elfSectionData.cpp
SHELL_VARIABLE_H=shell/variableTypes/elfVariableData.h shell/variableTypes/rawVariableData.h shell/variableTypes/arrayData.h shell/variableTypes/elfSectionData.h
SHELL_COMMANDS_SRC=shell/commands/command.cpp shell/commands/loadCommand.cpp shell/commands/saveCommand.cpp shell/commands/replaceCommand.cpp shell/commands/dwarfscriptCommand.cpp shell/commands/shellCommand.cpp shell/commands/infoCommand.cpp  shell/commands/hashCommand.cpp
SHELL_COMMANDS_H=shell/commands/command.h shell/commands/loadCommand.h shell/commands/saveCommand.h shell/commands/replaceCommand.h shell/commands/dwarfscriptCommand.h shell/commands/shellCommand.h shell/commands/infoCommand.h shell/commands/hashCommand.cpp
DWARFSCRIPT_SRC=shell/dwarfscript/dwarfscript.yy.c shell/dwarfscript/dwarfscript.tab.c
DWARFSCRIPT_H=shell/dwarfscript/dwarfscript.tab.h
SHELL_SRC=shell/shell.cpp  shell/shell.yy.cpp shell/shell.tab.cpp shell/param.cpp shell/variable.cpp shell/arrayAccessParam.cpp $(SHELL_COMMANDS_SRC) $(SHELL_VARIABLE_SRC) $(DWARFSCRIPT_SRC)
SHELL_H=shell/shell.h shell/parse_helper.h shell/shell.tab.h shell/param.h shell/variable.h  shell/arrayAccessParam.h $(SHELL_COMMANDS_SRC) $(SHELL_VARIABLE_H) $(DWARFSCRIPT_H)
INFO_SRC=info/fdedump.c info/dwinfo_dump.c  info/unsafe_funcs_dump.c
INFO_H=info/fdedump.h info/dwinfo_dump.h  info/unsafe_funcs_dump.h
REWRITER_SRC=rewriter/rewrite.c
REWRITER_H=rewriter/rewrite.h

H_FILES=dwarftypes.h elfparse.h elfutil.h types.h dwarf_instr.h register.h relocation.h symbol.h fderead.h dwarfvm.h config.h arch.h constants.h leb.h callFrameInfo.h elfwriter.h eh_pe.h $(PATCHER_H) $(PATCHWRITE_H) $(UTIL_H) $(INFO_H) $(REWRITER_H) $(SHELL_H)

EXTRA_DIST=$(H_FILES)

katana_SOURCES=katana.c dwarftypes.c   elfparse.c elfutil.c  types.c  dwarf_instr.c register.c relocation.c symbol.c fderead.c dwarfvm.c config.c leb.c callFrameInfo.c commandLine.c  elfwriter.c eh_pe.c $(PATCHWRITE_SRC) $(PATCHER_SRC) $(UTIL_SRC) $(INFO_SRC) $(REWRITER_SRC) $(SHELL_SRC)

BFLAGS=-d -v

