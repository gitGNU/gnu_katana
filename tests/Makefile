
include Makefile_vars
export CXX
export CC
export CFLAGS
export EXEC

SUBDIRS=test1_basic test2_nested_types test3_multiple_cu
.PHONY: $(SUBDIRS)

all : $(SUBDIRS)

$(SUBDIRS) :
	make -C $@ all


clean :
	-for d in $(SUBDIRS); do (cd $$d; make clean ); done