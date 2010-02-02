/*
  File: patchread.h
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Read patch object files and apply them
*/

#ifndef patchread_h
#define patchread_h
void readAndApplyPatch(int pid,ElfInfo* targetBin,ElfInfo* patch);
#endif
