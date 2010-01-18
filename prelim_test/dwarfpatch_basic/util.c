/*
  File: util.c
  Author: James Oakley
  Project:  Katana
  Date: January 10
  Description: misc utility macros/functions/structures
*/

#include "util.h"

void* zmalloc(size_t size)
{
  void* result=malloc(size);
  MALLOC_CHECK(result);
  memset(result,0,size);
  return result;
}
