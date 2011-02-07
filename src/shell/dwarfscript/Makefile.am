BFLAGS=-d -v
dwarfscript.yy.h : dwarfscript.l
	flex $<

dwarfscript.yy.cpp : dwarfscript.l
	flex $<

dwarfscript.tab.c : dwarfscript.y
	bison $(BFLAGS) -o dwarfscript.tab.c $<


dwarfscript.tab.h : dwarfscript.y
	bison $(BFLAGS) -o dwarfscript.tab.cpp $<


all-local: dwarfscript.yy.c dwarfscript.tab.c

clean-local:
	rm -f dwarfscript.yy.* dwarfscript.tab.*