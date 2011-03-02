/*

  FILE: dictionary.h
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
    
  Project:  Katana
  Date: February, 2008
  Modified: Jan, 2010
  Description: structure and prototypes for a general-purpose string-keyed dictionary
*/

#ifndef DICTIONARY_H
#define DICTIONARY_H


#include "util.h"
#include <stdbool.h>
#include "refcounted.h"

typedef struct _DNODE {
  struct _DNODE  *next;
  struct _DNODE  *prev;
  void    *data;        //  actual data 
  char* key; //  actual (URL) key
  unsigned int hash;//hash value for the key
} __DNODE;

typedef struct _DNODE DNODE;



typedef struct _DICTIONARY {
  RefCounted rc;
  DNODE **hash; //no longer a fixed-length array, allow this to be a general-purpose dictionary with user-defined size
  int numBuckets;//size of structure hash points to. Must remain constant due to current hash implementation
  DNODE *start;
  DNODE *end;
  int size; //number of elements in the dictionary
  
} __DICTIONARY;

typedef struct _DICTIONARY DICTIONARY;
typedef struct _DICTIONARY Dictionary;
typedef void* (*DictDataCopy)(void*);
typedef void (*DictDataDelete)(void*);

//! create an empty dictionary
Dictionary* dictCreate(int nBuckets);

//If dataCP is NULL then values will simply be copied over as pointers
Dictionary* dictDuplicate(Dictionary* dict,void* (*dataCP)(void*));

//! clean up a dictionary that's no longer needed
//! If deleteData is non-NULL it is called for each data element
void dictDelete(Dictionary* dict,DictDataDelete deleteData);

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

//refcounting functions
//both return the reference count
//release DOES NOT FREE ANY MEMORY
//you must call dictDelete yourself
int dictGrab(Dictionary* dict);
int dictRelease(Dictionary* dict);

#endif
