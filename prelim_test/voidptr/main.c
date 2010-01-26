/*
  File: main.c
  Author: James Oakley
  Project:  Katana (preliminary testing)
  Date: January 10
  Description:  Test generated dwarf information for unsafe use of void*
*/

#include <stdio.h>

typedef struct _Foo
{
  int m1;
  int m2;
} Foo;

void* bar;

void foo1()
{
  //no type information about the Foo type is generated in this subprogram
  //therefore, I cannot think of a good way to patch the data in bar if Foo changes
  //if the next access to bar after the patch occurs here
  int a=((Foo*)bar)->m1;
  printf("a is %i\n",a);
}



void foo2()
{
  //because we assign bar to a variable of type Foo, it should theoretically be possible
  //to instrument this function to check where the address of bar ends up, identify
  //that with the variable type Foo, and perform the appropriate transformation.
  Foo* a=(Foo*)bar;
  int b=a->m1;
  printf("b is %i\n",b);
}

int main(int argc,char** argv)
{
  Foo mumble;
  mumble.m1=4;
  mumble.m2=2;
  bar=&mumble;
  foo1();
  foo2();
  foo1();
}
