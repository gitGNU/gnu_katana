/*
  File: list.h
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: generic linked list
*/

#ifndef list_h
#define list_h

typedef struct _List
{
  void* value;
  struct _List* next;
} List;

List* concatLists(List* l1Start,List* l1End,List* l2Start,List* l2End,List** endOut);
void deleteList(List* start,void (*delFunc)(void*));

//pass the old head of the list and get the new head of the list
//no memory is changed
List* sortList(List* lst,int (*cmpfunc)(void*,void*));

int listLength(List* lst);

void listAppend(List** head,List** tail,List* li);
#endif
