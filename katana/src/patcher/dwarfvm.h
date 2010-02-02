/*
  File: dwarfvm.h
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Virtual Machine for executing Dwarf DW_CFA instructions
*/

#ifndef dwarfvm_h
#define dwarfvm_h
#include "types.h"
#include "fderead.h"
void patchDataWithFDE(VarInfo* var,FDE* transformerFDE,ElfInfo* targetBin);

#endif
