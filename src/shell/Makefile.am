SUBDIRS=dwarfscript
BFLAGS=-d -v
shell.yy.h : shell.l
	flex $<

shell.yy.cpp : shell.l shell.tab.hpp
	flex $<

shell.tab.cpp : shell.y
	bison $(BFLAGS) -o shell.tab.cpp $<


shell.tab.hpp : shell.y
	bison $(BFLAGS) -o shell.tab.cpp $<


all-local: shell.yy.cpp shell.tab.cpp

clean-local:
	rm -f shell.yy.* shell.tab.*