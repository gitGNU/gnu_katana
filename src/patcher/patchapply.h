/*
  File: patchread.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: January 2010
  Description: Read patch object files and apply them
*/

#ifndef patchapply_h
#define patchapply_h
void readAndApplyPatch(int pid,ElfInfo* targetBin,ElfInfo* patch);

#endif
