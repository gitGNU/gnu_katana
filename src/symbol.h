/*
  File: symbol.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Functions for dealing with symbols in ELF files
*/

#ifndef symbol_h
#define symbol_h

#include "types.h"
#include "elfparse.h"

addr_t getSymAddress(ElfInfo* e,int symIdx);
//return false if symbol with the given index doesn't exist
bool getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym);

//find the symbol matching the given symbol
int findSymbol(ElfInfo* e,GElf_Sym* sym,ElfInfo* ref);

GElf_Sym symToGELFSym(Elf32_Sym sym);

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return -1 if it cannot be found
//The matching is done solely on name
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx);

#endif

