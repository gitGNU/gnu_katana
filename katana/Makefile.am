SUBDIRS=src tests code_tests

TESTS=code_tests/listsort ./run_tests.py

all-local : src/katana
	if [ ! -x katana ]; then ln -s src/katana katana ;fi