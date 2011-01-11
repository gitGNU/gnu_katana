/*
  File: elfutil.h
  Author: James Oakley
  Copyright (C): 2011 Dartmouth College
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
  Date: January 2011
  Description: Misc. routines for working with elf files
*/

#ifndef elfutil_h
#define elfutil_h

#include "elfparse.h"

void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
void setWordAtAbs(Elf_Scn* scn,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
word_t getWordAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getDataAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getTextDataAtRelOffset(ElfInfo* e,int offset);
word_t getTextAtRelOffset(ElfInfo* e,int offset);
void printSymTab(ElfInfo* e);

//returns NULL if the section does not exist
Elf_Scn* getSectionByName(ElfInfo* e,char* name);
Elf_Data* getDataByIdx(ElfInfo* e,idx_t idx);
Elf_Data* getDataByERS(ElfInfo* e,E_RECOGNIZED_SECTION scn);
Elf_Scn* getSectionByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers);
void getShdrByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers,GElf_Shdr* shdr);
void getShdr(Elf_Scn* scn,GElf_Shdr* shdr);
char* getSectionNameFromIdx(ElfInfo* e,int idx);
//idx should be the index in the section header string table, not the
//section index
char* getScnHdrString(ElfInfo* e,int idx);
char* getString(ElfInfo* e,int idx);//get a string from the normal string table
char* getDynString(ElfInfo* e,int idx);//get a string from the dynamic string table
bool hasERS(ElfInfo* e,E_RECOGNIZED_SECTION ers);
//the returned string should be freed
char* getFunctionNameAtPC(ElfInfo* elf,addr_t pc);

#endif
