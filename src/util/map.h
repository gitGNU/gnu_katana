/*

  FILE: map.h
  Author: James Oakley
  Copyright (C): Portions 2010 Dartmouth College
                 Portions 2009 James Oakley
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
    
  Project:  Katana
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
