/*

  FILE: map.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Date: May, 2009
  Description: structure and prototypes for a general-purpose mapping
  from objects of any type to objects of any type

*/

#ifndef MAP_H
#define MAP_H


#include "util.h"
#include <stdbool.h>


typedef struct _M_DNODE {
  struct _M_DNODE  *next;
  struct _M_DNODE  *prev;
  void* data;        //  actual data 
  void* key; //  actual key
  unsigned int hash;//hash value for the key
} MNODE;


typedef struct _MAP {
  MNODE **hash; //array of all buckets
  int numBuckets;//size of structure hash points to. Must remain constant due to current hash implementation
  MNODE *start;
  MNODE *end;
  int size; //number of elements in the map
  unsigned long (*hashfunc)(void*);//hash function on key type
  int (*cmpfunc)(void*,void*);//comparison function on key type
} __MAP;

typedef unsigned int uint;

typedef struct _MAP MAP;
typedef struct _MAP Map;

//! create an empty map with the given number of buckets
//! hashfunc should be a function to has the key
//! type cmpfunc should be a function to compare two keys, returning
//! <0 if the first argument is less than the second, 0 if they are
//! equal, and >0 if the first argument is greater
Map* mapCreate(int bucketCnt,unsigned long (*hashfunc)(void*),int (*cmpfunc)(void*,void*));

//! helper function for creating a map
//! with int (well, int*), keys
Map* integerMapCreate(int bucketCount);

//! helper function for creating a map
//! with uint (well, uint*), keys
Map* uintMapCreate(int bucketCount);

//! helper function for creating a map
//! with size_t (well, size_t*), keys
Map* size_tMapCreate(int bucketCount);

//! clean up a map that's no longer needed
//! If deleteData is non-NULL it is called for each data element
//! If deleteKeys is non-NULL it is called for each key
void mapDelete(Map* map,void (*deleteData)(void*),void (*deleteKeys)(void*));

//!check if a key exists in the map
bool mapExists(const Map* map,void* key);

//! add an element to a map
//! Behavior is undefined if the key already exists in the map
void mapInsert(Map* map,void* key,void* value);


//! insert an element if the key does not already exist, otherwise
//! update it if deleteData is non-NULL, will be called with an old
//! value being overwritten. deleteKey is used on one copy of the key in the
//! event that updating an old value
void mapSet(Map* map,void* key,void* value,void (*deleteData)(void*),void (*deleteKey)(void*));

//! return the data associated with the key. NULL if the key does not exist in the map
void* mapGet(const Map* map,void* key);

//! remove the data associated with the key
//! If deleteData is non-NULL it is called for the data element
//! If deleteKeys is non-NULL it is called for the stored key
//! returns false if wasn't able to actually remove anything
bool mapRemove(Map* map,void* key,void (*deleteData)(void*),void (*deleteKeys)(void*));

//! returns the number of elements in the map
int mapSize(const Map* map);

//null-terminated array of all keys in map
//memory for has been malloced, should be freed when you're finished with it
//don't free elements
void** mapKeys(const Map* map);

//! prints everything in a map, given user supplied print functions
//! for keys and data
void mapPrint(const Map* map,void (*pfuncKey)(void*),void (*pfuncData)(void*));
#endif
