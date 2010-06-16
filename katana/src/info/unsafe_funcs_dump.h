/*
  File: unsafe_funcs_dump.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: March 2010
  Description: Print a list of functions which are unsafe, patching
               will not occur while they have activation fames on the stack
*/

#ifndef unsafe_funcs_dump_h
#define unsafe_funcs_dump_h
#include "elfparse.h"
void printPatchUnsafeFuncsInfo(ElfInfo* patch);

#endif
