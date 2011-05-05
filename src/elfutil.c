/*
  File: elfutil.c
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

#include "elfutil.h"
#include "util/logging.h"
#include "symbol.h"
#include "util/cxxutil.h"
#include "elfwriter.h"


void* getDataAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type)
{
  assert(scn);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  Elf_Data* data=elf_getdata(scn,NULL);
  assert(data->d_size >= offset);
  return data->d_buf+offset;
}

void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  return textData->d_buf+offset;
  //todo: merge this func with getDataAtAbs?
}

word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  return *((word_t*)(textData->d_buf+offset));
  //todo: merge this func with getWordAtAbs?
}

word_t getWordAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type)
{
  assert(scn);
  Elf_Data* data=elf_getdata(scn,NULL);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  assert(data->d_size >= offset);//todo: shouldn't it actually be offset plus size of word?
                                 //doing that was causing oddness though
  return *((word_t*)(data->d_buf+offset));
}

void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  memcpy(textData->d_buf+offset,&value,sizeof(word_t));
}

void setWordAtAbs(Elf_Scn* scn,addr_t addr,word_t value,ELF_STORAGE_TYPE type)
{
  assert(scn);
  Elf_Data* data=elf_getdata(scn,NULL);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  assert(data->d_size >= offset+sizeof(word_t));
  memcpy(data->d_buf+offset,&value,sizeof(word_t));
}

void* getTextDataAtRelOffset(ElfInfo* e,int offset)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  return textData->d_buf+offset;
}

word_t getTextAtRelOffset(ElfInfo* e,int offset)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  return *((word_t*)(textData->d_buf+offset));
}



void printSymTab(ElfInfo* e)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"symbol table:\n");
  /* print the symbol names */
  for (int i = 0; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
    gelf_getsym(symTabData, i, &sym);
    logprintf(ELL_INFO_V4,ELS_MISC,"%i. %s\n", i,elf_strptr(e->e, e->strTblIdx, sym.st_name));
  }

}

Elf_Data* getDataByIdx(ElfInfo* e,idx_t idx)
{
  Elf_Scn* scn=elf_getscn(e->e,idx);
  assert(scn);
  return elf_getdata(scn,NULL);
}

Elf_Data* getDataByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  return elf_getdata(scn,NULL);
}

Elf_Scn* getSectionByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  return scn;
}

void getShdrByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers,GElf_Shdr* shdr)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  if(!gelf_getshdr(scn,shdr))
  {
    death("Could not get shdr for section in getShdrByERS\n");
  }
}

Elf_Scn* getSectionByName(ElfInfo* e,char* name)
{
  assert(e->sectionHdrStrTblIdx);
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    GElf_Shdr shdr;
    if(!gelf_getshdr(scn,&shdr))
    {
      death("cannot get shdr\n");
    }
    char* scnName=getScnHdrString(e,shdr.sh_name);
    if(!strcmp(name,scnName))
    {
      return scn;
    }
  }
  return NULL;
}

char* getSectionNameFromIdx(ElfInfo* e,int idx)
{
  Elf_Scn* scn=elf_getscn(e->e,idx);
  if(!scn)
  {
    return NULL;
  }
  GElf_Shdr shdr;
  gelf_getshdr(scn,&shdr);
  return getScnHdrString(e,shdr.sh_name);
}


char* getString(ElfInfo* e,int idx)
{
  Elf_Scn* scn=elf_getscn(e->e,e->strTblIdx);
  Elf_Data* data=elf_getdata(scn,NULL);
  return (char*)data->d_buf+idx;
}

char* getDynString(ElfInfo* e,int idx)
{
  Elf_Scn* scn=getSectionByERS(e,ERS_DYNSTR);
  Elf_Data* data=elf_getdata(scn,NULL);
  return (char*)data->d_buf+idx;
}

//idx should be the index in the section header string table, not the
//section index
char* getScnHdrString(ElfInfo* e,int idx)
{
  assert(e);
  assert(e->sectionHdrStrTblIdx);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionHdrStrTblIdx);
  assert(scn);
  Elf_Data* data=elf_getdata(scn,NULL);
  assert(data);
  assert(idx<data->d_size);
  return (char*)data->d_buf+idx;
}

void getShdr(Elf_Scn* scn,GElf_Shdr* shdr)
{
  if(!gelf_getshdr(scn,shdr))
  {
    death("gelf_getshdr failed\n");
  }
}

bool hasERS(ElfInfo* e,E_RECOGNIZED_SECTION ers)
{
  assert(e);
  return 0!=e->sectionIndices[ers];
}

//the returned string should be freed
char* getFunctionNameAtPC(ElfInfo* elf,addr_t pc)
{
  idx_t symIdx=findSymbolContainingAddress(elf,pc,STT_FUNC,SHN_UNDEF);
  if(STN_UNDEF==symIdx)
  {
    return "?";
  }
  GElf_Sym sym;
  getSymbol(elf,symIdx,&sym);
  char* name=getString(elf,sym.st_name);
  if(name[0]=='_' && name[1]=='Z')
  {
    //the name is a C++ mangled name, or at least it very likely is
    //(there is of course nothing stopping someone from naming a
    //function starting with _Z in C).
    return demangleName(name);
  }
  else
  {
    return strdup(name);
  }
}

void updateShdrFromSectionHeaderData(ElfInfo* e,SectionHeaderData* shd,GElf_Shdr* shdr)
{
  
  //we do not know if the string used by this section could
  //theoretically be used in full or in part by another
  //section. Therefore we have no choice but to create a new string in
  //the string table if the strings differ
  char* oldScnName=getScnHdrString(e,shdr->sh_name);
  if(strcmp(oldScnName,shd->name))
  {
    shdr->sh_name=addShdrStrtabEntry(e,shd->name);
  }
  shdr->sh_type=shd->sh_type;
  shdr->sh_flags=shd->sh_flags;
  if(shd->sh_offset)
  {
    //there's an offset to set directly
    shdr->sh_offset=shd->sh_offset;
  }
  else
  {
    //this is a really tricky situation. We don't let libelf
    //recalculate all of the layout because for whatever reason it
    //tends to screw it up. Therefore we have to change the offset
    //ourselves. Really to be thorough we should calculate all offsets
    //in a pass after we've set everything up. For the moment,
    //however, we'll just adjust the offset by the same amount we did
    //the address. We do have to keep them in sync because of segment loading.
    shdr->sh_offset=shdr->sh_offset+(shd->sh_addr-shdr->sh_addr);
  }
  shdr->sh_addr=shd->sh_addr;

  shdr->sh_size=shd->sh_size;
  shdr->sh_link=shd->sh_link;
  shdr->sh_info=shd->sh_info;
  shdr->sh_addralign=shd->sh_addralign;
  shdr->sh_entsize=shd->sh_entsize;
}


SectionHeaderData gshdrToSectionHeaderData(ElfInfo* e,GElf_Shdr shdr)
{
  SectionHeaderData shd;
  char* scnName=getScnHdrString(e,shdr.sh_name);
  if(strlen(scnName)>sizeof(shd.name))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Section name %s too long, will be truncated in extract command\n",scnName);
  }
  strncpy(shd.name,scnName,sizeof(shd.name)-1);

  shd.sh_type=shdr.sh_type;
  shd.sh_flags=shdr.sh_flags;
  shd.sh_addr=shdr.sh_addr;
  shd.sh_offset=shdr.sh_offset;
  shd.sh_size=shdr.sh_size;
  shd.sh_link=shdr.sh_link;
  shd.sh_info=shdr.sh_info;
  shd.sh_addralign=shdr.sh_addralign;
  shd.sh_entsize=shdr.sh_entsize; 
  return shd;
}
