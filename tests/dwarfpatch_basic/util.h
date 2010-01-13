/*
  File: util.h
  Author: James Oakley
  Project:  Katana
  Date: January 10
  Description: misc utility macros/functions/structures
*/


#ifndef UTIL_H
#define UTIL_H

#include <string.h> //for memset in BZERO
#include <stdio.h> //for fprintf
#include <stdlib.h>

typedef struct _List
{
  void* value;
  struct _List* next;
} List;


//malloc, check return value, and zero
void* zmalloc(size_t size);

#define min(x,y)   ((x)>(y))?(y):(x)

//! \brief Check whether \a s is NULL or not on a memory allocation. Quit this program if it is NULL.
#define MALLOC_CHECK(s)  if ((s) == NULL)   {                     \
    fprintf(stderr,"No enough memory at %s:line%d ", __FILE__, __LINE__); \
    perror(":");                                                  \
    exit(-1); \
  }

//! \brief Set memory space starts at pointer \a n of size \a m to zero. 
#define BZERO(n,m)  memset(n, 0, m)



#endif
