#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "util/list.h"

void swap(char* a,char* b)
{
  char tmp=*a;
  *a=*b;
  *b=tmp;
}

int cmpChar(void* a,void* b)
{
  return (*(char*)a)-(*(char*)b);
}

char* printList(List* lst)
{
  int len=256;
  char* buf=malloc(len);
  if(!lst)
  {
    return "{}";
  }
  int cnt=0;
  cnt+=snprintf(buf,len,"{");
  while(lst)
  {
    cnt+=snprintf(buf+cnt,len,"%c%s",*(char*)lst->value,lst->next?",":"");
    lst=lst->next;
  }
  snprintf(buf+cnt,len,"}");
  return buf;
}

int main(int argc,char** argv)
{
  char* data=malloc(26);
  List* lst=NULL;
  List* lstTail=NULL;
  for(int i=0;i<26;i++)
  {
    List* li=malloc(sizeof(List));
    li->next=NULL;
    li->value=data+i;
    if(!lst)
    {
      lst=lstTail=li;
    }
    else
    {
      lstTail->next=li;
    }
    lstTail=li;
    data[i]='z'-i;
  }
  //mix it up a little so sorting has to do something besides *just* reverse the list
  swap(data,data+25);
  swap(data+3,data+9);
  lst=sortList(lst,&cmpChar);
  int cnt=0;
  while(lst)
  {
    if(*(char*)lst->value!='a'+cnt)
    {
      fprintf(stderr,"listsort: Found %c instead of %c at location %i\n",*(char*)lst->value,'a'+cnt,cnt);
      exit(-1);
    }
    lst=lst->next;
    cnt++;
  }
  assert(26==cnt);
}
