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
#include <libdwarf.h>
//caller should free info when no longer needs it
DwarfInfo* readDWARFTypes(ElfInfo* elf);

//return false if the two types are not
//identical in all regards
//if the types are not identical, return
//transformation information necessary
//to convert from type a to type b
//todo: add in this transformation info
bool compareTypes(TypeInfo* a,TypeInfo* b,TypeTransform** transform);

void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg);

#endif
