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

void fixupVariable(VarInfo var,TransformationInfo* trans,int pid);
#endif
