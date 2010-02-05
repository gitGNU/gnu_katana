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

void die(char* reason)
{
  if(reason)
  {
    fprintf(stderr,"%s",reason);
  }
  fflush(stderr);
  fflush(stdout);
  abort();
}

void death(char* reason)
{
  die(reason);
}

void deleteList(List* start,void (*delFunc)(void*))
{
  List* li=start;
  while(li)
  {
    if(delFunc)
    {
      (*delFunc)(li->value);
    }
    List* tmp=li->next;
    free(li);
    li=tmp;
  }
}

List* concatLists(List* l1Start,List* l1End,List* l2Start,List* l2End,List** endOut)
{
  if(!l1Start && !l2Start)
  {
    if(endOut)
    {
      *endOut=NULL;
    }
    return NULL;
  }

  if(l1Start && !l1End)
  {
    l1End=l1Start;
    while(l2End->next)
    {
      l2End=l2End->next;
    }
  }
  if(l2Start && !l2End)
  {
    l2End=l2Start;
    while(l2End->next)
    {
      l2End=l2End->next;
    }
  }

  if(!l1Start)
  {
    if(endOut)
    {
      *endOut=l2End;
    }
    return l2Start;
  }
  if(!l2Start)
  {
    if(endOut)
    {
      *endOut=l1End;
    }
    return l1Start;
  }

  l1End->next=l2Start;
  
  if(endOut)
  {
    *endOut=l2End;
  }
  return l1Start;
}
