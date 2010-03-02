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
  int* field1;
  int field_extra;
  int* field2;
  int* field3;
  struct _Foo* other;
} Foo;

Foo alpha;
Foo beta;
Foo* gamma;
int vals1[3]={1,2,3};
int vals2[3]={33,22,11};
int vals3[3]={44,55,66};


#define MILLISECOND 1000

void printFoo(char* name,Foo* foo)
{
  printf("name is %s\n",name);
  fflush(stdout);
  printf("%s lives at 0x%x\n",name,(unsigned int)foo);
  printf("layout is {%x,%x,%x,%x,%x}\n",(unsigned int)foo->field1,(unsigned int)foo->field_extra,(unsigned int)foo->field2,(unsigned int)foo->field3,(unsigned int)foo->other);
  fflush(stdout);
  printf("%s: %i, %i, %i\n",name,*(foo->field1),*(foo->field2),*(foo->field3));
  if(foo->other)
  {
    char buf[256];
    snprintf(buf,256,"%s.other",name);
    printFoo(buf,foo->other);
  }
  else
  {
    printf("%s has no other member\n",name);
  }
}

void printThings()
{
  printf("printThings new version starting\n");
  fflush(stdout);
  printFoo("alpha",&alpha);
  printFoo("beta",&beta);
  printFoo("gamma",gamma);
  fflush(stdout);
}

void assignValues()
{
  alpha.field1=&vals1[0];
  alpha.field2=&vals1[1];
  alpha.field3=&vals1[2];
  beta.field1=&vals2[0];
  beta.field2=&vals2[1];
  beta.field3=&vals2[2];
  alpha.other=&beta;
  beta.other=NULL;
  gamma=malloc(sizeof(Foo));
  gamma->field1=&vals3[0];
  gamma->field2=&vals3[1];
  gamma->field3=&vals3[2];
  gamma->other=&alpha;
}

int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  assignValues();
  while(1)
  {
    printThings();
    usleep(100*MILLISECOND);
  }
  return 0;
}
