/*
  File: list.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: generic linked list
*/

#include "list.h"
#include <stdlib.h>
#include <assert.h>

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

void deleteDList(DList* start,void (*delFunc)(void*))
{
  DList* li=start;
  while(li)
  {
    if(delFunc)
    {
      (*delFunc)(li->value);
    }
    DList* tmp=li->next;
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

int listLength(List* lst)
{
  int cnt=0;
  while(lst)
  {
    cnt++;
    lst=lst->next;
  }
  return cnt;
}

List* mergeSortedLists(List* a,List* b,int (*cmpfunc)(void*,void*))
{
  List* result=NULL;
  List* resultTail=NULL;
  while(a && b)
  {
    List* next;
    if((*cmpfunc)(a->value,b->value)<=0)
    {
      next=a;
      a=a->next;
    }
    else
    {
      next=b;
      b=b->next;
    }
    next->next=NULL;
    if(!result)
    {
      result=resultTail=next;
    }
    else
    {
      resultTail->next=next;
      resultTail=next;
    }
  }
  
  while(a) //must be !b otherwise above while loop would still be going
  {
    if(!result)
    {
      result=a;
    }
    else
    {
      resultTail->next=a;
      resultTail=a;
    }
    a=a->next;
  }

  while(b)
  {
    if(!result)
    {
      result=b;
    }
    else
    {
      resultTail->next=b;
      resultTail=b;
    }
    b=b->next;
  }
  return result;
}

//pass the old head of the list and get the new head of the list
//no memory is changed
List* sortList(List* lst,int (*cmpfunc)(void*,void*))
{
  //do mergesort
  
  //todo: not efficient. Really should have a list container
  //and keep track of the head, tail, length, etc
  int halflen=listLength(lst)/2;
  if(halflen<1)
  {
    //only 1 or 0 elements in the list
    return lst;
  }
  List* middle=lst;
  List* beforeMiddle=NULL;
  for(int i=0;i<halflen;i++)
  {
    beforeMiddle=middle;
    middle=middle->next;
  }
  beforeMiddle->next=NULL;//separate the two lists so we can sort them separately
  List* a=sortList(lst,cmpfunc);
  List* b=sortList(middle,cmpfunc);
  return mergeSortedLists(a,b,cmpfunc);
}

void listAppend(List** head,List** tail,List* li)
{
  assert(head);
  assert(tail);
  li->next=NULL;
  if(*head)
  {
    (*tail)->next=li;
  }
  else
  {
    *head=li;
  }
  *tail=li;
}

void listPush(List** head,List** tail,List* li)
{
  assert(head);
  li->next=*head;
  if(NULL==*head && tail)
  {
    *tail=li;
  }
  *head=li;
}

void dlistPush(DList** head,DList** tail,DList* li)
{
  assert(head);
  li->prev=NULL;
  li->next=*head;
  if(li->next)
  {
    li->next->prev=li;
  }
  if(NULL==*head && tail)
  {
    *tail=li;
  }
  *head=li;
}

void dlistAppend(DList** head,DList** tail,DList* li)
{
  assert(head);
  assert(tail);
  li->next=NULL;
  li->prev=*tail;
  if(*head)
  {
    assert(*tail);
    (*tail)->next=li;
  }
  else
  {
    *head=li;
  }
  *tail=li;
}

void dlistDeleteTail(DList** head,DList** tail)
{
  assert(head);
  assert(tail);
  assert(*tail);
  DList* oldTail=*tail;
  *tail=oldTail->prev;
  if(!*tail)
  {
    *head=NULL;
  }
  free(oldTail);
}
