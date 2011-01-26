/*
  File: util.c
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
  Description: misc utility macros/functions/structures
*/

#include "util.h"
#include <stdarg.h>
#include <assert.h>

void* zmalloc(size_t size)
{
  void* result=malloc(size);
  MALLOC_CHECK(result);
  memset(result,0,size);
  return result;
}


void death(const char* reason,...)
{
  va_list ap;
  va_start(ap,reason);
  if(reason)
  {
    vfprintf(stderr,reason,ap);
  }
  fflush(stderr);
  fflush(stdout);
  abort();
}

bool strEndsWith(char* str,char* suffix)
{
  int suffixLen=strlen(suffix);
  int strLen=strlen(str);
  if(suffixLen > strLen)
  {return false;}
  int start=strLen-suffixLen;
  return !strcmp(str+start,suffix);
}

uint64_t signExtend32To64(uint32_t val)
{
  uint64_t result=val;
  if(val & (1 << 31))
  {
    memset(((byte*)&result)+4,0xFF,4);
  }
  return result;
}

//sign-extends val based on fromWhichByte which should be set to how
//many bytes of val are filled. Assumes unfilled bytes of val are 0.
size_t sextend(size_t val,int fromWhichByte)
{
  if(fromWhichByte==sizeof(val))
  {
    return val;
  }
  assert(fromWhichByte<sizeof(val));
  int mask=(1<<(8*fromWhichByte-1));
  if(val&mask)
  {
    memset(((byte*)&val)+fromWhichByte,0xFF,sizeof(val)-fromWhichByte);
  }
  return val;
}


//get a hexadecimal string version of binary string
char* getHexDataString(byte* data,int len)
{
  char* buf=malloc(len*2+1);
  MALLOC_CHECK(buf);
  for(int i=0;i<len;i++)
  {
    sprintf(buf+i*2,"%02x",data[i]);
  }
  return buf;
}
