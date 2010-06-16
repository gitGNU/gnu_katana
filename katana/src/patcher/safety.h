/*
  File: safety.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: March 2010
  Description: Determines when it is safe to patch and sets a breakpoint at the next safe point
*/

#ifndef safety_h
#define safety_h

void printBacktrace(ElfInfo* elf,int pid);

//find a location in the target where nothing that's being patched is being used.
addr_t findSafeBreakpointForPatch(ElfInfo* targetBin,ElfInfo* patch,int pid);

void bringTargetToSafeState(ElfInfo* targetBin,ElfInfo* patch,int pid);
#endif
