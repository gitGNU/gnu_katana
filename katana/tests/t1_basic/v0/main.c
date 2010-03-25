/*
  File: main_v0.c
  Author: James Oakley
  Project: Katana - Preliminary Work
  Date: January 10
  Description: Very simple program that exists to have one of its data
               types patched. main_v1.c is the same thing with an extra
               field in one of the types
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

typedef struct _Foo
{
  int field1;
  int field2;
  int field3;
} Foo;

Foo bar={42,66,111};

#define MILLISECOND 1000

void printThings()
{
  printf("Foo: %i,%i\n",bar.field1,bar.field2);
  printf("field 1 at addr: %lx, field 2 at addr: %lx\n",(unsigned long) &bar.field1,(unsigned long)&bar.field2);
  fflush(stdout);
}

int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    usleep(100*MILLISECOND);
  }
  return 0;
}
