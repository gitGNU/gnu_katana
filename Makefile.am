SUBDIRS= src tests doc

#add ./apache_test when it is ready
TESTS=tests/code/listsort ./run_dwarf_tests.sh ./patch_unit_tests

EXTRA_DIST=LICENSE $(TESTS) validator.py

all-local : src/katana
	if [ ! -x katana ]; then ln -s src/katana katana ;fi