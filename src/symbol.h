/*
  File: symbol.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: Functions for dealing with symbols in ELF files
*/

#ifndef symbol_h
#define symbol_h

#include "types.h"
#include "elfparse.h"
#include "arch.h"

addr_t getSymAddress(ElfInfo* e,int symIdx);
//gets the symbol at the given idx, dies if it doesn't exist
void getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym);

typedef enum
{
  ESFF_MANGLED_OK=1,//permit matching mangled names
  ESFF_NEW_DYNAMIC=2,//look in the dynamic symbol table instead for the new symbol
  ESFF_DYNAMIC=ESFF_NEW_DYNAMIC,//will be OR'd NEW_DYNAMIC and
                                //OLD_DYNAMIC when OLD_DYNAMIC IS
                                //implemented
  ESFF_VERSIONED_SECTIONS_OK=4,//permit fuzzy matching names in versioned sections
  ESFF_BSS_MATCH_DATA_OK=8,
  ESFF_FUZZY_MATCHING_OK=ESFF_MANGLED_OK | ESFF_VERSIONED_SECTIONS_OK
} E_SYMBOL_FIND_FLAGS;

//find the symbol matching the given symbol
idx_t findSymbol(ElfInfo* e,GElf_Sym* sym,ElfInfo* ref,int flags);

GElf_Sym nativeSymToGELFSym(ElfXX_Sym sym);
ElfXX_Sym gelfSymToNativeSym(GElf_Sym);

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return -1 if it cannot be found
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx,int flags);

//flags is OR'd E_SYMBOL_FIND_FLAGS
//only ESFF_MANGLED_OK and ESFF_DYNAMIC are relevant
int getSymtabIdx(ElfInfo* e,char* symbolName,int flags);

//pass SHN_UNDEF for scnIdx to accept symbols referencing any section
idx_t findSymbolContainingAddress(ElfInfo* e,addr_t addr,byte type,idx_t scnIdx);
#endif

