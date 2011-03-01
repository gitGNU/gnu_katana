/*
  File: stack.c
  Author: James Oakley
  Copyright (C): 2010 James Oakley
  License: Katana is free software: you may redistribute it and/or
     modify it under the terms of the GNU General Public License as
     published by the Free Software Foundation, either version 2 of the
     License, or (at your option) any later version.

     This file was not written while under employment by Dartmouth College
  Project:  katana
  Date: October 2010
  Description: abstract stack data type
*/

#include "stack.h"

typedef struct SNode
{
  struct SNode* next;
  void* data;
} SNode;


Stack* stackCreate()
{
  return zmalloc(sizeof(Stack));

}

void stackDelete(Stack* st,void (*deleteData)(void*))
{
  if(deleteData)
  {
    for(SNode* node=st->top;node;node=node->next)
    {
      (*deleteData)(node->data);
    }
  }
  free(st);
}

void stackPush(Stack* st,void* item)
{
  SNode* node=zmalloc(sizeof(SNode));
  node->next=st->top;
  node->data=item;
  st->top=node;
}

//return NULL if there is no item at the top of the stack
void* stackPop(Stack* st)
{
  SNode* result=st->top;
  if(!result)
  {
    return NULL;
  }
  st->top=result->next;
  return result->data;;
}


