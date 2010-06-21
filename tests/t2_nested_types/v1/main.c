/*
  File: main.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: Test program to demonstrate patching a modified type
               which itself has a member which is a modified type
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

struct Foo
{
  int field1;
  int field_extra;
  int field2;
};

struct Bar
{
  int field1;
  struct Foo foo;
  int field2;
  int field3;
};

void printThings();
int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    usleep(1000000);
  }
  return 0;
}

struct Bar alpha={42,{111,0,128},69,0};
  
void printThings()
{
  printf("alpha: %i,(foo: %i,%i),%i\n",alpha.field1,alpha.foo.field1,alpha.foo.field2,alpha.field2);
  printf("v1: field 1 at addr: %lx, foo.field1 at %lx, field 2 at %lx\n",(unsigned long)&alpha.field1,(unsigned long)&alpha.foo.field1,(unsigned long)&alpha.field2);
  printf("v1: foo.field_extra is %i and field3 is %i\n",alpha.foo.field_extra,alpha.field3);
  fflush(stdout);
}


