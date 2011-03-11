#!/bin/sh

check()
{
  if [ $? -ne 0 ]; then
    echo "  DWARF test $1 failed"
    exit 1
  fi
}

doTest()
{
  cd $1 > /dev/null
  check $1
  ./test.sh > /dev/null
  check $1
  cd ..
  echo "  DWARF test $1 passed"
}

doTest passthrough
doTest identity
