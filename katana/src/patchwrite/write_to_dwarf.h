/*
  File: write_to_dwarf.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: March 2010
  Description: methods for writing the dwarf information part of the patch
*/

#ifndef write_to_dwarf_h
#define write_to_dwarf_h
#include "types.h"
void writeVarToDwarf(Dwarf_P_Debug dbg,VarInfo* var,CompilationUnit* cu,bool new);
void writeTransformationToDwarf(Dwarf_P_Debug dbg,TypeTransform* trans);
void writeFuncToDwarf(Dwarf_P_Debug dbg,char* name,uint textOffset,uint funcTextSize,
                      int symIdx,CompilationUnit* cu,bool new);
#endif
