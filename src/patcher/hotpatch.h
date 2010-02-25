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
addr_t getFreeSpaceForTransformation(TransformationInfo* trans,uint howMuch);
void performRelocations(ElfInfo* e,VarInfo* var);
#endif
