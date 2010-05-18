/*
  File: v0/main.c
  Author: James Oakley
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
  int* field1;
  int field2;
} Foo;

typedef struct _Bar
{
  int idx;
  Foo foo;
} Bar;

typedef union _Baz
{
  int i;
  Foo f;
  Bar b;
} Baz;


typedef enum
{
  VAL_B,
  VAL_C
} Moo;

void printThings();
void assignValues();
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

void printFoo(char* name,Foo* foo)
{
  fflush(stdout);
  printf("%s lives at 0x%lx\n",name,(unsigned long)foo); 
  if(foo->field1)
  {
    printf("%s: %i, %i\n",name,*(foo->field1),foo->field2);
  }
  else
  {
    printf("%s: NULL, %i\n",name,foo->field2);
  }
}


void printBar(char* name,Bar* bar)
{
  printf("%s idx is %i\n",name,bar->idx);
  char buf[64];
  sprintf(buf,"%s.foo",name);
  printFoo(buf,&bar->foo);
}


Baz alpha; //will use the int member of the union
Baz beta;  //will use the Foo member of the union
Baz gamma; //will use the Bar member of the union
Moo epsilon=VAL_B;


void printThings()
{
  printf("alpha is %i\n",alpha.i);
  printFoo("beta",&beta.f);
  printBar("gamma",&gamma.b);
  printf("sizeof(Baz) is %zu\n",sizeof(Baz));
  fflush(stdout);
}

void assignValues()
{
  alpha.i=42;
  beta.f.field1=malloc(sizeof(int));
  *beta.f.field1=43;
  beta.f.field2=44;
  gamma.b.idx=45;
  gamma.b.foo.field1=beta.f.field1;
  gamma.b.foo.field2=46;
}
