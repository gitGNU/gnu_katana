/*
  File: v0/main.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

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
