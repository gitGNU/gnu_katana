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

#include <libelf.h>
#include <gelf.h>
#include "types.h"

struct ElfInfo;
typedef struct ElfInfo ElfInfo;

typedef enum
{
  IN_MEM=1,
  ON_DISK=2
} ELF_STORAGE_TYPE;

typedef enum
{
  ERS_TEXT=1,
  ERS_RODATA,
  ERS_DATA,
  ERS_SYMTAB,
  ERS_STRTAB,
  ERS_HASHTABLE,
  ERS_RELA_TEXT,
  ERS_REL_TEXT,
  ERS_GOT,
  ERS_GOTPLT,
  ERS_PLT,
  ERS_RELX_PLT,
  ERS_DYNSYM,
  ERS_DYNSTR,
  ERS_DYNAMIC,
  ERS_UNSAFE_FUNCTIONS,
  ERS_DEBUG_INFO,
  ERS_EH_FRAME,
  ERS_CNT,
  ERS_INVALID
} E_RECOGNIZED_SECTION;


//the data necessary to actually construct an ElfXX_Shdr
//the reason for this structures existence is that it stores an actualy name
//field rather than a stringtable offset. This is important if the header
//data is not yet connected to an actual ELF file
typedef struct SectionHeaderData
{
  char name[256];
  word_t sh_type;
  word_t sh_flags;
  word_t sh_addr;
  word_t sh_offset;
  word_t sh_size;
  word_t sh_link;
  word_t sh_info;
  word_t sh_addralign;
  word_t sh_entsize;
} SectionHeaderData;

////////////////////////////////////////
//methods for getting pieces of raw data
void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
void setWordAtAbs(Elf_Scn* scn,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
word_t getWordAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getDataAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getTextDataAtRelOffset(ElfInfo* e,int offset);
word_t getTextAtRelOffset(ElfInfo* e,int offset);


////////////////////////////////////////
//methods for getting whole sections or data blocks
//returns NULL if the section does not exist
Elf_Scn* getSectionByName(ElfInfo* e,char* name);
Elf_Scn* getSectionByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers);
Elf_Data* getDataByIdx(ElfInfo* e,idx_t idx);
Elf_Data* getDataByERS(ElfInfo* e,E_RECOGNIZED_SECTION scn);


////////////////////////////////////////
//methods for working with section headers
void getShdrByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers,GElf_Shdr* shdr);
void getShdr(Elf_Scn* scn,GElf_Shdr* shdr);
void updateShdrFromSectionHeaderData(ElfInfo* e,SectionHeaderData* shd,GElf_Shdr* shdr);
SectionHeaderData gshdrToSectionHeaderData(ElfInfo* e,GElf_Shdr shdr);

//////////////////////////////////////////
//misc
char* getSectionNameFromIdx(ElfInfo* e,int idx);
//idx should be the index in the section header string table, not the
//section index
char* getScnHdrString(ElfInfo* e,int idx);
char* getString(ElfInfo* e,int idx);//get a string from the normal string table
char* getDynString(ElfInfo* e,int idx);//get a string from the dynamic string table
bool hasERS(ElfInfo* e,E_RECOGNIZED_SECTION ers);
//the returned string should be freed
char* getFunctionNameAtPC(ElfInfo* elf,addr_t pc);
void printSymTab(ElfInfo* e);


#endif
