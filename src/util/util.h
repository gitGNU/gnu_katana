/*
  File: util.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  Katana
  Date: January 10
  Description: misc utility macros/functions/structures
*/


#ifndef UTIL_H
#define UTIL_H

#include <string.h> //for memset in BZERO
#include <stdio.h> //for fprintf
#include <stdlib.h>
#include <stdbool.h>
#include <elf.h>

typedef unsigned char byte;

//malloc, check return value, and zero
void* zmalloc(size_t size);


void death(char* reason,...);

#define min(x,y)   ((x)>(y))?(y):(x)

#define max(x,y)   ((x)<(y))?(y):(x)

//! \brief Check whether \a s is NULL or not on a memory allocation. Quit this program if it is NULL.
#define MALLOC_CHECK(s)  if ((s) == NULL)   {                     \
      perror("system error:");                                                  \
      death("No enough memory at %s:line %d ", __FILE__, __LINE__); \
  }

//! \brief Set memory space starts at pointer \a n of size \a m to zero. 
#define BZERO(n,m)  memset(n, 0, m)

#define GARBAGE 0xCC //204 decimal

bool strEndsWith(char* str,char* suffix);

uint64_t signExtend32To64(uint32_t val);

#endif
