/*
  File: typediff.h
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: functions relating to finding difference between types and building
               transformers for them
*/

#ifndef typediff_h
#define typediff_h

#include "types.h"

//return false if the two types are not
//identical in all regards
//if the types are not identical, store in type a
//the necessary transformation info to convert it to type b,
//if possible
bool compareTypesAndGenTransforms(TypeInfo* a,TypeInfo* b);
#endif
