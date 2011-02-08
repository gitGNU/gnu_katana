#!/bin/sh

check()
{
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

../../../katana recompile.ksh
check
diff test.dws test_mod.dws
check

