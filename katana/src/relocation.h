/*
  File: relocation.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description:  methods to deal with relocation and by association with some symbol issues
*/

#ifndef relocation_h
#define relocation_h

#include "elfparse.h"
#include <gelf.h>

//we always store the addend for relocations, it's just easier that way
typedef struct
{
  addr_t r_offset;
  idx_t symIdx;//extracted from r_info
  byte relocType;//extracted from r_info
  addr_t r_addend;
  ElfInfo* e;//the elf object the relocation is for
  int scnIdx;//which section this relocation applies to in e
} RelocInfo;

//todo: I don't think this struct is currently used
//for anything --james
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

//get the relocation entry at the given offset from the start of relocScn
RelocInfo* getRelocationEntryAtOffset(ElfInfo* e,Elf_Scn* relocScn,addr_t offset);
//modify the relocation entry at the given offset from the start of relocScn
void setRelocationEntryAtOffset(RelocInfo* reloc,Elf_Scn* relocScn,addr_t offset);


//apply the given relocation
//type determines whether the relocation is being applied
//in-memory or on-disk or both
void applyRelocation(RelocInfo* rel,ELF_STORAGE_TYPE type);

//apply a list of relocations (list value type is GElf_Reloc)
void applyRelocations(List* relocs,ELF_STORAGE_TYPE type);


//apply all relocations in an executable
//oldElf is used for reference, what things that
//need relocating were originally located against
void applyAllRelocations(ElfInfo* e,ElfInfo* oldElf);

//compute an addend for when we have REL instead of RELA
//scnIdx is section relocation is relative to
addr_t computeAddend(ElfInfo* e,byte type,idx_t symIdx,addr_t r_offset,idx_t scnIdx);

//if the reloc has an addend, return it, otherwise compute it
addr_t getAddendForReloc(RelocInfo* reloc);

//todo: does this belong in this module?
addr_t getPLTEntryForSym(ElfInfo* e,int symIdx);

//get the section containing relocations for the given function
//if want only the general relocation section, pass null for function name
//return NULL if there is no relocation section
Elf_Scn* getRelocationSection(ElfInfo* e,char* fnname);

#endif
