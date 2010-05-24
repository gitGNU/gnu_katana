/*
  File: elfwriter.c
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: February 2010
  Description: routines for building an elf file for a patch. Not thread safe
               or safe for creating multiple patches at the same time
*/

#ifndef elfwriter_h
#define elfwriter_h

#include "elfparse.h"
#include "arch.h"

//must be called before any other routines
//for each patch object to create
ElfInfo* startPatchElf(char* fname);

//adds data to a section and returns the offset of that
//data in the section
addr_t addDataToScn(Elf_Data* dataDest,void* data,int size);

//adds an entry to the string table, return its offset
int addStrtabEntry(char* str);
//return index of entry in symbol table
int addSymtabEntry(Elf_Data* data,ElfXX_Sym* sym);

int reindexSectionForPatch(ElfInfo* e,int scnIdx);

//must be called at the end of each patch and
//before a new patch can be started
//it may *NOT* be called in the middle of creating a patch
void endPatchElf();

int dwarfWriteSectionCallback(char* name,int size,Dwarf_Unsigned type,
                              Dwarf_Unsigned flags,Dwarf_Unsigned link,
                              Dwarf_Unsigned info,int* sectNameIdx,int* error);
#endif
