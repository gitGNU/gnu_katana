/*
  File: relocation.h
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description:  methods to deal with relocation and by association with some symbol issues
*/

#ifndef relocation_h
#define relocation_h

#include "elfparse.h"
#include <gelf.h>

typedef enum
{
  ERT_REL,
  ERT_RELA,
} E_RELOC_TYPE;

typedef struct
{
  GElf_Rel rel;//the relocation
  GElf_Rela rela;
  E_RELOC_TYPE type;//only one of rel and rela is valid. This tells which one
  ElfInfo* e;//the elf object the relocation is for
  int scnIdx;//which section this relocation applies to in e
} RelocInfo;

typedef struct
{
  ElfInfo* oldElf;
  ElfInfo* newElf;
  int oldSymIdx;
  int newSymIdx;
} SymMoveInfo;

//List should be freed when you're finished with it
//list value type is RelocInfo
List* getRelocationItemsFor(ElfInfo* e,int symIdx);

//get relocation items that live in the given relocScn
//that are for  in-memory addresses between lowAddr and highAddr inclusive
//list value type is RelocInfo
List* getRelocationItemsInRange(ElfInfo* e,Elf_Scn* relocScn,addr_t lowAddr,addr_t highAddr);

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return -1 if it cannot be found
//The matching is done solely on name
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx);

/*
//apply all relocations corresponding to the movement of certain symbols
//The List parameter should be a List with values of type
//SymMoveInfo. Relocations are applied into the newElf fields
//in the SymMoveInfo structs
void applyRelocationsForSymbols(List* symMoves);*/

//apply the given relocation using oldSym for reference
//to calculate the offset from the symol address
//oldSym may be NULL if the relocation type is ERT_RELA instead of ERT_REL
//type determines whether the relocation is being applied
//in-memory or on-disk or both
void applyRelocation(RelocInfo* rel,GElf_Sym* oldSym,ELF_STORAGE_TYPE type);

//apply a list of relocations (list value type is GElf_Reloc)
//oldElf is the elf object containing the symbol information
//that items were originally located against. This is necessary
//to compute the offsets from symbols
void applyRelocations(List* relocs,ElfInfo* oldElf);

//apply all relocations in an executable
//oldElf is used for reference, what things that
//need relocating were originally located against
void applyAllRelocations(ElfInfo* e,ElfInfo* oldElf);
#endif
