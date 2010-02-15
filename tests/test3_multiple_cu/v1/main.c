/*
  File: main.c v1
  Author: James Oakley
  Project: Katana - Test Harness
  Date: January 10
  Description: Very simple program that exists to have one of its data
               types patched. main_v1.c is the same thing with an extra
               field in one of the types
*/
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

typedef struct _Foo
{
  int field1;
  int field4;
  int field2;
  int field3;
} Foo;

Foo bar={42,66,111};


void sigReceived(int signum)
{
  fprintf(stderr,"bad signal\n");
  abort();
}

void otherPrintThings();

void printThings()
{
  int a=1;
  printf("This is print things version %i\n",a);
  printf("Foo: %i,%i\n",bar.field1,bar.field2);
  printf("field 1 at addr: %x, field 2 at addr: %x\n",(unsigned int) &bar.field1,(unsigned int)&bar.field2);
  fflush(stdout);
}

int main(int argc,char** argv)
{
  struct sigaction act;
  act.sa_handler=&sigReceived;
  memset(&act,0,sizeof(act));
  sigaction(SIGSEGV,&act,NULL);
  sigaction(SIGILL,&act,NULL);
  sigaction(SIGTERM,&act,NULL);
  sigaction(SIGQUIT,&act,NULL);
  sigaction(SIGHUP,&act,NULL);
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    otherPrintThings();
    usleep(100000);
  }
  return 0;
}
