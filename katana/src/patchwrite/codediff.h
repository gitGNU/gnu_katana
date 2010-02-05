/*
  File: codediff.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Determine if two functions are identical
*/

#ifndef codediff_h
#define codediff_h
bool areSubprogramsIdentical(SubprogramInfo* patcheeFunc,SubprogramInfo* patchedFunc,
                             ElfInfo* oldBinary,ElfInfo* newBinary);
#endif
