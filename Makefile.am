SUBDIRS=src tests code_tests

TESTS=code_tests/listsort ./run_tests.py

all-local : src/katana
	if [ ! -x katana ]; then ln src/katana katana ;fi