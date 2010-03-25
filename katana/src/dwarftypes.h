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
//workingDir is used for path names
//it is the directory that names should be relative to
DwarfInfo* readDWARFTypes(ElfInfo* elf,char* workingDir);


void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg);

#endif
