#!/bin/sh

check()
{
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

doTest()
{
  cd $1 > /dev/null
  check
  ./test.sh > /dev/null
  check
  cd ..
  echo "  DWARF test $1 passed"
}

doTest passthrough
doTest identity
