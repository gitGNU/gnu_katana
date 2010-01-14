/*
  File: hotpatch.h
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: functions for actually modifying the target and performing the hotpatching
*/

#ifndef hotpatch_h
#define hotpatch_h
#include "types.h"

//return false if the two types are not
//identical in all regards
//if the types are not identical, return
//transformation information necessary
//to convert from type a to type b
//todo: add in this transformation info
bool compareTypes(TypeInfo* a,TypeInfo* b,TypeTransform** transform);
void fixupVariable(VarInfo var,TransformationInfo* trans,int pid);
#endif
