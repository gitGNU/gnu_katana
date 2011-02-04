#!/bin/sh

check()
{
  if [ $? -ne 0 ]; then
    exit 1
  fi
}

doTest()
{
  cd $1
  ./test.sh > /dev/null
  check
  cd ..
  echo Dwarf test $1 passed
}

doTest passthrough
