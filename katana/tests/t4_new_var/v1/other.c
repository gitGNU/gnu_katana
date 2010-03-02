/*
  File: other.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: test of adding a new variable Separated into its own file because of
               limitations in the current version of katana that will be fixed later 
*/
#include <stdio.h>
typedef struct Foo_
{
  int field1;
  int field2;
} Foo;

Foo alpha={42,66};
int beta=24;

void printThings()
{
  printf("v1: alpha is {%i,%i}\n",alpha.field1,alpha.field2);
  printf("v1: beta is %i\n",beta);
  fflush(stdout);
}

