/*
  File: v0/main.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
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
  Description: Very simple program that exists to have its data types patched
               types patched. v1/main.c is the same thing with an extra
               field in one of the types
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
  int* field2;
  int* field3;
  struct _Foo* other;
} Foo;

void assignValues();
void printThings();
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


Foo alpha;
Foo beta;
Foo* gamma;
int vals1[3]={1,2,3};
int vals2[3]={33,22,11};
int vals3[3]={44,55,66};

void printFoo(char* name,Foo* foo)
{
  printf("name is %s\n",name);
  fflush(stdout);
  printf("%s lives at 0x%zx\n",name,(size_t)foo);
  printf("layout is {%lx,%lx,%lx,%lx}\n",(unsigned long)foo->field1,(unsigned long)foo->field2,(unsigned long)foo->field3,(unsigned long)foo->other);
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
  printf("printThings old version starting\n");
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

