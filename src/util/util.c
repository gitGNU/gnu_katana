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
