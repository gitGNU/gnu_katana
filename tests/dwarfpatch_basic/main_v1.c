/*
  File: main_v1.c
  Author: James Oakley
  Project: Katana - Preliminary Work
  Date: January 10
  Description: Very simple program that exists to patch a data type in main_v0.
               main_v0.c is the same thing with one less field in the Foo type
*/
#include <stdio.h>
typedef struct
{
  int field1;
  int field_extra;
  int field2;
} Foo;

Foo bar={42,0,66};

int main(int argc,char** argv)
{
  printf("Foo: %i,%i\n",bar.field1,bar.field2);
  printf("field 1 at addr: %x, field 2 at addr: %x\n",(unsigned int)&bar.field1,(unsigned int)&bar.field2);
  return 0;
}
