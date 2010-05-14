#File: configure.ac
#Author: James Oakley
#Tested with Automake 1.10 on Mac OSX (Darwin) and Ubuntu Linux
#This is the configure.ac file for the phitautext project

AC_DEFUN([doMissingLibrary],[
  echo "A required library is missing, abort"
  exit 1
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
AC_CONFIG_FILES([Makefile src/Makefile])

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

AC_OUTPUT

