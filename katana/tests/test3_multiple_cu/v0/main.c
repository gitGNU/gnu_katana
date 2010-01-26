/*
  File: main.c v0
  Author: James Oakley
  Project: Katana - Testing
  Date: January 2010
  Description: Very simple program that exists to have one of its data
               types patched. main_v1.c is the same thing with an extra
               field in one of the types
*/
#include <stdio.h>
#include <unistd.h>
typedef struct _Foo
{
  int field1;
  int field2;
  int field3;
} Foo;

Foo bar={42,66,111};

int value1=5;

void otherPrintThings();

void printThings()
{
  printf("Foo: %i,%i\n",bar.field1,bar.field2);
  printf("field 1 at addr: %x, field 2 at addr: %x\n",(unsigned int) &bar.field1,(unsigned int)&bar.field2);
  fflush(stdout);
}

int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    otherPrintThings();
    usleep(100000);
  }
  return 0;
}
