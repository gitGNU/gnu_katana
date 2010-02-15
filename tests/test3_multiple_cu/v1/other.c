/*
  File: other2.c
  Author: James Oakley
  Project: Katana - Testing
  Date: January 2010
  Description: different definition of struct Foo than in main.c to make sure
               patching system treats the different versions properly
*/


#include <stdio.h>

typedef struct _Foo
{
  int field3;
  int field1;
  int field2;
} Foo;

Foo bar2={31,13};


void otherPrintThings()
{
  int a=1;
  printf("This is otherPrintThings version %i\n",a);
  printf("(Other) Foo: %i,%i\n",bar2.field1,bar2.field2);
  printf("(Other) field 1 at addr: %x, field 2 at addr: %x\n",(unsigned int) &bar2.field1,(unsigned int)&bar2.field2);
  fflush(stdout);
}

