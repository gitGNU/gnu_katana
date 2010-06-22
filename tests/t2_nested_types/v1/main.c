/*
  File: main.c
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

  Project: Katana
  Date: February 2010
  Description: Test program to demonstrate patching a modified type
               which itself has a member which is a modified type
*/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

struct Foo
{
  int field1;
  int field_extra;
  int field2;
};

struct Bar
{
  int field1;
  struct Foo foo;
  int field2;
  int field3;
};

void printThings();
int main(int argc,char** argv)
{
  printf("has pid %i\n",getpid());
  while(1)
  {
    printThings();
    usleep(1000000);
  }
  return 0;
}

struct Bar alpha={42,{111,0,128},69,0};
  
void printThings()
{
  printf("alpha: %i,(foo: %i,%i),%i\n",alpha.field1,alpha.foo.field1,alpha.foo.field2,alpha.field2);
  printf("v1: field 1 at addr: %lx, foo.field1 at %lx, field 2 at %lx\n",(unsigned long)&alpha.field1,(unsigned long)&alpha.foo.field1,(unsigned long)&alpha.field2);
  printf("v1: foo.field_extra is %i and field3 is %i\n",alpha.foo.field_extra,alpha.field3);
  fflush(stdout);
}


