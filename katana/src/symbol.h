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
//gets the symbol at the given idx, dies if it doesn't exist
void getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym);

typedef enum
{
  ESFF_MANGLED_OK=1,//permit matching mangled names
  ESFF_NEW_DYNAMIC=2,//look in the dynamic symbol table instead for the new symbol
  ESFF_VERSIONED_SECTIONS_OK=4,//permit fuzzy matching names in versioned sections
  ESFF_FUZZY_MATCHING_OK=ESFF_MANGLED_OK | ESFF_VERSIONED_SECTIONS_OK
} E_SYMBOL_FIND_FLAGS;

//find the symbol matching the given symbol
int findSymbol(ElfInfo* e,GElf_Sym* sym,ElfInfo* ref,int flags);

GElf_Sym nativeSymToGELFSym(Elf32_Sym sym);
Elf32_Sym gelfSymToNativeSym(GElf_Sym);

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return -1 if it cannot be found
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx,int flags);

int getSymtabIdx(ElfInfo* e,char* symbolName);
#endif

