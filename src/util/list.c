/*
  File: list.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

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
      result=resultTail=a;
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
      result=resultTail=b;
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
