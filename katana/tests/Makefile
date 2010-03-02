
include Makefile_vars
export CXX
export CC
export CFLAGS
export EXEC

SUBDIRS=t1_basic t2_nested_types t3_multiple_cu t4_new_var t5_pointers
.PHONY: $(SUBDIRS)

all : $(SUBDIRS)

$(SUBDIRS) :
	make -C $@ all


clean :
	-for d in $(SUBDIRS); do (cd $$d; make clean ); done