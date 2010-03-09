/*
  File: refcounted.h
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: structure for refcounted objects
*/


#ifndef refcounted_h
#define refcounted_h
#include <assert.h>

typedef struct
{
  int refcount;
} RefCounted;

typedef RefCounted RC;

typedef void (*FreeFunc)(void*);
void grabRefCounted(RefCounted* rc);
void releaseRefCounted(RefCounted* rc,FreeFunc freeFunc);
#endif