#!/bin/sh

check()
{
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

../../../katana recompile.ksh
check
./test_mod > log
check
diff log test.out
check

