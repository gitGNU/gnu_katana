/*

  FILE: dictionary.h
  Author: James Oakley
  Date: February, 2008
  Modified: Jan, 2010
  Description: structure and prototypes for a general-purpose string-keyed dictionary
*/

#ifndef DICTIONARY_H
#define DICTIONARY_H


#include "util.h"
#include <stdbool.h>


typedef struct _DNODE {
  struct _DNODE  *next;
  struct _DNODE  *prev;
  void    *data;        //  actual data 
  char* key; //  actual (URL) key
  unsigned int hash;//hash value for the key
} __DNODE;

typedef struct _DNODE DNODE;



typedef struct _DICTIONARY {
  DNODE **hash; //no longer a fixed-length array, allow this to be a general-purpose dictionary with user-defined size
  int numBuckets;//size of structure hash points to. Must remain constant due to current hash implementation
  DNODE *start;
  DNODE *end;
  int size; //number of elements in the dictionary
} __DICTIONARY;

typedef struct _DICTIONARY DICTIONARY;
typedef struct _DICTIONARY Dictionary;

//! create an empty dictionary
Dictionary* dictCreate(int nBuckets);

//! clean up a dictionary that's no longer needed
//! If deleteData is non-NULL it is called for each data element
void dictDelete(Dictionary* dict,void (*deleteData)(void*));

//!check if a key exists in the dictionary
bool dictExists(const Dictionary* dict,char* key);

//! add an element to a dictionary
//! Behavior is undefined if the key already exists in the dictionary
void dictInsert(Dictionary* dict,char* key,void* value);

//! insert an element if the key does not already exist, otherwise set its data
//! if deleteData is non-NULL, will be called with an old value being overwritten
void dictSet(Dictionary* dict,char* key,void* value,void (*deleteData)(void*));

//! return the data associated with the key. NULL if the key does not exist in the dictionary
void* dictGet(const Dictionary* dict,char* key);

//! returns the number of elements in the dictionary
int dictSize(const Dictionary* dict);

//! prints everything in a dictionary, given a user-supplied print function to handle the data
void dictPrint(const Dictionary* dict,void (*pfunc)(void*));

//null-terminated array of all keys in dictionary
//memory for array has been malloced, should be freed when you're finished with it
//don't free elements
char** dictKeys(const Dictionary* dict);

//null-terminated array of all values in dictionary
//memory for array has been malloced, should be freed when you're finished with it
//don't free elements
void** dictValues(const Dictionary* dict);

#endif
