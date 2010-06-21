/*
  File: v0/main.c
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: January 10
  Description: Very simple program that exists to have one of its data
               types patched. v1/main.c is the same thing with slight
               changes to its data types
*/
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#define MILLISECOND 1000

typedef struct _Foo
{
  int field1;
  int field2;
  int field3;
} Foo;


const int alpha=2;
const Foo beta={42,43,44};


void printThings()
{
  printf("alpha: %i\n",alpha);
  printf("beta: %i,%i,%i\n",beta.field1,beta.field2,beta.field3);
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
