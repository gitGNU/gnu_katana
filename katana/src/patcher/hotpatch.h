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
#ifdef legacy
addr_t getFreeSpaceForTransformation(TransformationInfo* trans,uint howMuch);
#endif
addr_t getFreeSpaceInTarget(uint howMuch);
#endif
