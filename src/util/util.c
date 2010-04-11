/*
  File: util.c
  Author: James Oakley
  Project:  Katana
  Date: January 10
  Description: misc utility macros/functions/structures
*/

#include "util.h"
#include <stdarg.h>

void* zmalloc(size_t size)
{
  void* result=malloc(size);
  MALLOC_CHECK(result);
  memset(result,0,size);
  return result;
}


void death(char* reason,...)
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
