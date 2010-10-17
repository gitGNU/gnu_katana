SUBDIRS= src tests code_tests doc

TESTS=code_tests/listsort ./patch_unit_tests ./apache_test

EXTRA_DIST=LICENSE $(TESTS) validator.py

all-local : src/katana
	if [ ! -x katana ]; then ln -s src/katana katana ;fi