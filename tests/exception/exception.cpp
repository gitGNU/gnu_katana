/*
  File: exception.c
  Author: James Oakley
  Project: Katana - Preliminary Testing
  Date: January 10
  Description: Test of C++ exception handling so I can see what things look like in gdb and for readelf/dwarfdump/etc
*/

#include <cstdio>

int foo()
{
  printf("foo\n");
}

int main(int argc,char** argv)
{
  try
  {
    printf("hello world\n");
    printf("now throwing an exception\n");
    throw 1;
    printf("shouldn't be here\n");
  }
  catch(int val)
  {
    printf("caught exception value of %i\n",val);
    foo();
  }
  printf("block finished\n");
  return 0;
}

