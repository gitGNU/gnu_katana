/*
  File: refcounted.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: reference counting methods
*/

#include "refcounted.h"

void grabRefCounted(RefCounted* rc)
{
  assert(rc->refcount>=0);
  rc->refcount++;
}

void releaseRefCounted(RefCounted* rc,FreeFunc freeFunc)
{
  assert(rc->refcount>0);
  rc->refcount--;
  if(0==rc->refcount)
  {
    (*freeFunc)(rc);
  }
}
