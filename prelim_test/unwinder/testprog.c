/*
  File: testprog.c
  Author: James Oakley
  Project: Katana - preliminary testing
  Date: January 10
  Description: test program for stack unwinding used by the unwinder program
*/


#include <stdio.h>
#include <unistd.h>

int tablevel=0;

void printTabs()
{
  for(int i=0;i<tablevel;i++)
  {
    printf("\t");
  }
}
  
void foo4(char* str)
{
  tablevel++;
  printTabs();
  printf("in foo4 %s\n",str);
  tablevel--;
}

void foo3(int a,int b)
{
  tablevel++;
  printTabs();
  printf("in foo3 %i,%i\n",a,b);
  foo4("foo!");
  printTabs();
  printf("end foo3\n");
  tablevel--;
}

void foo2()
{
  tablevel++;
  printTabs();
  printf("in foo2\n");
  for(int i=0;i<4;i++)
  {
    foo3(i,i);
  }
  foo4("mumble");
  printTabs();
  printf("end foo2\n");
  tablevel--;
}

void foo1()
{
  tablevel++;
  printTabs();
  printf("in foo1\n");
  foo3(42,24);
  foo4("bar");
  foo2();
  printTabs();
  printf("end foo1\n");
  tablevel--;
}



int main(int argc,char** argv)
{
  while(1)
  {
    printf("addresses are:\n");
    printf("foo1: 0x%x\n",(unsigned int)&foo1);
    printf("foo2: 0x%x\n",(unsigned int)&foo2);
    printf("foo3: 0x%x\n",(unsigned int)&foo3);
    printf("foo4: 0x%x\n",(unsigned int)&foo4);
    printf("call foo1\n");
    foo1();
    printf("call foo2\n");
    foo2();
    printf("call foo1 again\n");
    foo1();
    fflush(stdout);
    sleep(3);
  }
}
