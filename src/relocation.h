/*
  File: relocation.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

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
