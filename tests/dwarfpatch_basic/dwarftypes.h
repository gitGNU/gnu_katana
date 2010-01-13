/*
  File: dwarftypes.h
  Author: James Oakley
  Project: Katana (Preliminary Work)
  Date: January 10
  Description: functions for reading/manipulating DWARF type information in an ELF file
*/


#ifndef dwarftypes_h
#define dwarftypes_h
#include "elfparse.h"
void readDWARFTypes(Elf* elf);

void fixupVariable(VarInfo var,TransformationInfo* trans,int pid);
#endif
