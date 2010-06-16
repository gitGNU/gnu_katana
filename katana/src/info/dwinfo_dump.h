/*
  File: dwinfo_dump.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: March 2010
  Description: Dumps patch information from the dwarf section of the patch
*/

#ifndef dwinfo_dump_h
#define dwinfo_dump_h
void printPatchDwarfInfo(ElfInfo* patch,Map* fdeMap);
#endif
