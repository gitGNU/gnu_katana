#File: configure.ac
#Author: James Oakley
#Tested with Automake 1.10 on Mac OSX (Darwin) and Ubuntu Linux
#This is the configure.ac file for the phitautext project

AC_DEFUN([doMissingLibrary],[
  AC_MSG_ERROR([A required library is missing, abort])
])


#we don't really want to do anything on successfully finding a library
#but autoconf is very strange and seems to automatically add the library to libs
#if we don't have some sort of action
AC_DEFUN([fndlibPlaceholder],[
  echo "required no-op" > /dev/null
])


AC_INIT([katana],[1.0b],[electron100@gmail.com])


AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_OBJC
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile code_tests/Makefile src/Makefile tests/Makefile tests/t1_basic/Makefile tests/t1_basic/v0/Makefile tests/t1_basic/v1/Makefile tests/t2_nested_types/Makefile tests/t2_nested_types/v0/Makefile tests/t2_nested_types/v1/Makefile tests/t3_multiple_cu/Makefile tests/t3_multiple_cu/v0/Makefile tests/t3_multiple_cu/v1/Makefile tests/t4_new_var/Makefile tests/t4_new_var/v0/Makefile tests/t4_new_var/v1/Makefile tests/t5_pointers/Makefile tests/t5_pointers/v0/Makefile tests/t5_pointers/v1/Makefile tests/t6_unions_enums/Makefile tests/t6_unions_enums/v0/Makefile tests/t6_unions_enums/v1/Makefile tests/t7_const/Makefile tests/t7_const/v0/Makefile tests/t7_const/v1/Makefile])

#set up variables for use in Makefile.am
AC_CHECK_LIB(elf,elf_begin,[fndlibPlaceholder],[doMissingLibrary])
AC_CHECK_LIB(dwarf,dwarf_dealloc,[fndlibPlaceholder],[doMissingLibrary])
#todo: should include check for libunwind-generic, but it seems to compile
#such that all its functions are prefixed differently on different platforms,
#so not sure what function to check for
#AC_CHECK_LIB(unwind-generic,[fndlibPlaceholder],[doMissingLibrary])
AC_CHECK_LIB(unwind-ptrace,_UPT_create,[fndlibPlaceholder],[doMissingLibrary])
AC_CHECK_LIB(m,ceil,[fndlibPlaceholder],[doMissingLibrary])


case $host_os in
     linux*) SUPPORTED_OS=yes ;;
     *) echo "Your operating system is not supported by Katana. Please contact the Katana maintainers if you would like this fixed"; exit 1 ;;
esac

case $host_cpu in
     x86_64) SUPPORTED_CPU=yes ;;
     x86) SUPPORTED_CPU=yes ;;
     i686) SUPPORTED_CPU=yes ;;
     *) echo "Your CPU architecture is not supported by Katana. Please contact the Katana maintainers if you would like this fixed"; exit 1
esac

AC_CHECK_PROG(HAVE_PYTHON,python,yes,[no])

if test x$HAVE_PYTHON != xyes; then
   AC_MSG_WARN(["No python on this system. Unit tests will not work"])
fi

AC_OUTPUT

