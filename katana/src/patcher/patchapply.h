/*
  File: patchread.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: January 2010
  Description: Read patch object files and apply them
*/

#ifndef patchapply_h
#define patchapply_h
typedef enum 
{
  PF_STOP_TARGET
} PATCHAPPLY_FLAGS;

void readAndApplyPatch(int pid,ElfInfo* targetBin,ElfInfo* patch,int flags);

#endif
