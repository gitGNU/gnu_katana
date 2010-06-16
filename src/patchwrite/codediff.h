/*
  File: codediff.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: Determine if two functions are identical
*/

#ifndef codediff_h
#define codediff_h
bool areSubprogramsIdentical(SubprogramInfo* patcheeFunc,SubprogramInfo* patchedFunc,
                             ElfInfo* oldBinary,ElfInfo* newBinary);
#endif
