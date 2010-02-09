/*
  File: main_v1.c
  Author: James Oakley
  Project: Katana - Preliminary Work
  Date: January 10
  Description: Very simple program that exists to patch a data type in main_v0.
               main_v0.c is the same thing with one less field in the Foo type
*/
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

typedef struct _Foo
{
  int field1;
  int field_extra;
  int field2;
  int field3;
} Foo;

Foo bar={42,0,66,111};

void sigReceived(int signum)
{
  fprintf(stderr,"bad signal\n");
  abort();
}


void printThings()
{
  int a=1;
  printf("this is print things version %i\n",a);
  printf("Foo: %i,%i\n",bar.field1,bar.field2);
  printf("field 1 at addr: %x, field 2 at addr: %x\n",(unsigned int)&bar.field1,(unsigned int)&bar.field2);
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
    usleep(100000);
  }
  return 0;
}
