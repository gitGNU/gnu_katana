/*
  File: symbol.c
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

