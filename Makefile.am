SUBDIRS= src tests doc


TESTS_ENVIRONMENT=PATH=$(PWD):$(PATH)
TESTS=tests/code/listsort ./run_dwarf_tests.sh  ./patch_unit_tests ./apache_test.sh

EXTRA_DIST=LICENSE $(TESTS) validator.py

all-local : src/katana
	if [ ! -x katana ]; then ln -s src/katana katana ;fi

SIGFILES_GZ=$(DIST_ARCHIVES:.gz=.gz.sig)
SIGFILES_BZ=$(SIGFILES_GZ:.bz2=.bz2.sig)
SIGFILES=$(SIGFILES_BZ) 

release : dist
	stat=`git status -uno -s`; if [[ $$stat ]]; then echo "commit your changes first"; exit 1; fi
	gpg -b $(DIST_ARCHIVES)
	git tag $(PACKAGE_VERSION)
	scp $(DIST_ARCHIVES) $(SIGFILES) electron@dl.sv.nongnu.org:/releases/katana/
#TODO: if anyone else ever gains admin rights on the project the
#name above will not always be electron

