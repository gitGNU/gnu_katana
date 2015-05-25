/*
  File: elfwriter.c
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
  Description: routines for building an elf file for a patch. Not thread safe
               or safe for creating multiple patches at the same time
*/

#ifndef elfwriter_h
#define elfwriter_h

#include "elfparse.h"
#include "arch.h"

//Must be called before any other routines for each patch object to
//create. Filename is just used for identification purposes it is not
//enforced to correspond to file. If file is NULL, however, a new file
//will be created at filename
ElfInfo* startPatchElf(FILE* file,char* filename);

//adds data to a section and returns the offset of that
//data in the section
addr_t addDataToScn(Elf_Data* dataDest, const void* data,int size);

//wipes out the existing information in dataDest and replaces it with data
void replaceScnData(Elf_Data* dataDest,void* data,int size);

//like replaceScnData except only modifies size amount of data
//starting at offset. If offset+size is longer than the current length
//of the data, extends it as necessary
void modifyScnData(Elf_Data* dataDest,word_t offset,void* data,int size);

//adds an entry to the string table, return its offset
int addStrtabEntry(ElfInfo* e, const char* str);

//adds an entry to the section header string table, return its offset
int addShdrStrtabEntry(ElfInfo* e,char* str);

//return index of entry in symbol table
int addSymtabEntry(ElfInfo* e,Elf_Data* data,ElfXX_Sym* sym);

int reindexSectionForPatch(ElfInfo* e,int scnIdx,ElfInfo* patch);

int dwarfWriteSectionCallback(const char* name,int size,Dwarf_Unsigned type,
                              Dwarf_Unsigned flags,Dwarf_Unsigned link,
                              Dwarf_Unsigned info,Dwarf_Unsigned* sectNameIdx,
                              void* user_data, int* error);

//prepare a modified elf object for writing
void finalizeModifiedElf(ElfInfo* e);
#endif
