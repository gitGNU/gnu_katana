/*
  File: dictionary.c
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
  Date: January 10. Based on earlier versions by James Oakley
  Description:  Map strings to arbitrary data
*/



#include "dictionary.h"
#include "hash.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

//! create an empty dictionary with the given number of buckets
Dictionary* dictCreate(int bucketCnt)
{
  Dictionary* dict=malloc(sizeof(Dictionary));
  MALLOC_CHECK(dict);
  BZERO(dict,sizeof(*dict));
  dict->hash=malloc(bucketCnt*sizeof(DNODE*));
  dict->numBuckets=bucketCnt;
  MALLOC_CHECK(dict->hash);
  BZERO(dict->hash,bucketCnt*sizeof(DNODE*));
  dict->size=0;
  dict->rc.refcount=1;
  return dict;
}

//helper function
DNODE* dictNodeDuplicate(DNODE* n,void* (*dataCP)(void*))
{
  if(!n)
  {
    return NULL;
  }
  DNODE* new=malloc(sizeof(DNODE));
  MALLOC_CHECK(new);
  new->key=strdup(n->key);
  if(dataCP)
  {
    new->data=(*dataCP)(n->data);
  }
  else
  {
    new->data=n->data;
  }
  new->hash=n->hash;
  new->prev=NULL;
  if(n->next)
  {
    new->next=dictNodeDuplicate(n->next,dataCP);
    new->next->prev=new;
  }
  else
  {
    new->next=NULL;
  }
  return new;
}

//If dataCP is NULL then values will simply be copied over as pointers
Dictionary* dictDuplicate(Dictionary* dict,void* (*dataCP)(void*))
{
  Dictionary* new=dictCreate(dict->numBuckets);
  new->size=dict->size;
  new->start=dictNodeDuplicate(dict->start,dataCP);
  //now have to find the end. Also have to fill in the hash list
  new->hash=malloc(new->numBuckets*sizeof(DNODE*));
  memset(new->hash,0,new->numBuckets*sizeof(DNODE*));
  DNODE* n=new->start;
  DNODE* prev=NULL;
  while(n)
  {
    if(!new->hash[n->hash])
    {
      new->hash[n->hash]=n;
    }
    prev=n;
    n=n->next;
  }
  new->end=prev;
  return new;
}

void dictDelete(Dictionary* dict,void (*deleteData)(void*))
{
  //clean up all items held in the hash table
  DNODE* tmp=dict->start;
  while(tmp)
  {
    DNODE* tmp2=tmp->next;
    if(deleteData)
    {
      (*deleteData)(tmp->data);
    }
    free(tmp->key);
    free(tmp);
    tmp=tmp2;
  }
  //free the memory for the table itself
  free(dict->hash);
  free(dict);
}

//!check if a key exists in the dictionary
bool dictExists(const Dictionary* dict,char* key)
{
  unsigned int hash=hash1(key)%dict->numBuckets;
  //printf("check existance key %s with hash of %u\n",key,hash);
  DNODE* node=dict->hash[hash];
  if(node)
  {
    do
    {
      if(!strcmp(key,node->key))
      {
        return true;
      }
    }while((node=node->next) && hash==node->hash);
  }
  return false;
}

//! add an element to a dictionary
void dictInsert(Dictionary* dict,char* key,void* value)
{ 
  assert(!dictExists(dict,key));
  DNODE* node=malloc(sizeof(DNODE));
  MALLOC_CHECK(node);
  BZERO(node,sizeof(DNODE));//shouldn't be necessary, but a good precaution
  node->data=value;
  int keySize=strlen(key)+1;
  node->key=malloc(keySize);
  MALLOC_CHECK(node->key);
  strcpy(node->key,key);
  
  node->hash=hash1(key)%dict->numBuckets;
  DNODE* parent=dict->hash[node->hash];
  if(!parent)
  {
    dict->hash[node->hash]=node;
    node->next=0;
    node->prev=dict->end;
    if(!dict->start)
    {
      dict->start=dict->end=node;
    }
    else
    {
      dict->end->next=node;
      dict->end=node;
    }
  }
  else
  {
    while(parent->next && parent->next->hash==node->hash)
    {
      parent=parent->next;
      assert(strcmp(parent->key,node->key));//don't want to insert the same element into the dictionary again
    }
    if(parent==dict->end)
    {
      dict->end=node;
    }
    
    
    node->next=parent->next;
    if(node->next)
    {
      node->next->prev=node;
    }
    node->prev=parent;
    parent->next=node;
  }
  dict->size++;
}

//! insert an element if the key does not already exist, otherwise update it
//! insert an element if the key does not already exist, otherwise set its data
//! if deleteData is non-NULL, will be called with an old value being overwritten
void dictSet(Dictionary* dict,char* key,void* value,void (*deleteData)(void*))
{
  assert(dict);
  unsigned int hash=hash1(key)%dict->numBuckets;
  DNODE* node=dict->hash[hash];
  if(node)
  {
    do
    {
      if(!strcmp(key,node->key))
      {
        if(deleteData)
        {
          (*deleteData)(node->data);
        }
        node->data=value;
        return;
      }
    }while((node=node->next) && hash==node->hash);
  }
  
  //since we already have the hash value computed, it would be slightly faster
  //to replicate most of the dictInsert code here ourselves
  //however the speed benefits are minimal so to save my time and effort,
  //we just call dictInsert. In a production/speed-critical system
  //I would either redo the code or construct this and dictInsert out of modular parts
  dictInsert(dict,key,value);
}

//! return the data associated with the key. NULL if the key does not exist in the dictionary
void* dictGet(const Dictionary* dict,char* key)
{
  assert(dict);
  unsigned int hash=hash1(key)%dict->numBuckets;
  DNODE* node=dict->hash[hash];
  if(node)
  {
    do
    {
      if(!strcmp(key,node->key))
      {
        return node->data;
      }
    }while((node=node->next) && hash==node->hash);
  }
  return NULL;
}

//! returns the number of elements in the dictionary
int dictSize(const Dictionary* dict)
{
  return dict->size;
}


//! prints out everything in a dictionary
void dictPrint(const Dictionary* dict,void (*pfunc)(void*))
{
  printf("Dictionary size: %i\nITEMS:\n",dict->size);
  DNODE* node=dict->start;
  for(;node;node=node->next)
  {
    printf("Key: %s, hash: %u\nValue:",node->key,node->hash);
    (*pfunc)(node->data);
    printf("\n");
  }
}

//null-terminated array of all keys in dictionary
//memory for has been malloced, should be freed when you're finished with it
//don't free elements
char** dictKeys(const Dictionary* dict)
{
  char** keys=malloc(sizeof(char*)*(dict->size+1));
  DNODE* node=dict->start;
  for(int i=0;node;node=node->next,i++)
  {
    keys[i]=node->key;
  }
  keys[dict->size]=0;
  return keys;
}

//null-terminated array of all values in dictionary
//memory for array has been malloced, should be freed when you're finished with it
//don't free elements
void** dictValues(const Dictionary* dict)
{
  void** vals=malloc(sizeof(char*)*(dict->size+1));
  DNODE* node=dict->start;
  for(int i=0;node;node=node->next,i++)
  {
    vals[i]=node->data;
  }
  vals[dict->size]=0;
  return vals;
}

//refcounting functions
//both return the reference count
//release DOES NOT FREE ANY MEMORY
//you must call dictDelete yourself
int dictGrab(Dictionary* dict)
{
  dict->rc.refcount++;
  return dict->rc.refcount;
}
int dictRelease(Dictionary* dict)
{
  dict->rc.refcount--;
  assert(dict->rc.refcount>=0);
  return dict->rc.refcount;
}
