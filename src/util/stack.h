/*
  File: stack.h
  Author: James Oakley
  Copyright (C): 2010 James Oakley
  License: Katana is free software: you may redistribute it and/or
     modify it under the terms of the GNU General Public License as
     published by the Free Software Foundation, either version 2 of the
     License, or (at your option) any later version.

     This file was not written while under employment by Dartmouth
     College and the attribution requirements on the rest of Katana do
     not apply to code taken from this file.
  Project:  katana
  Date: October 2010
  Description: abstract stack data type
*/

#ifndef stack_h
#define stack_h

#include "util.h"

typedef struct _Stack
{
  struct SNode* top;
} Stack;


Stack* stackCreate();

void stackDelete(Stack* st,void (*deleteData)(void*));

void stackPush(Stack* st,void* item);

//return NULL if there is no item at the top of the stack
void* stackPop(Stack* st);

#endif
