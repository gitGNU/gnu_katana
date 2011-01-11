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

#include "relocation.h"
#include "util/util.h"
#include "patcher/target.h"
#include <assert.h>
#include "symbol.h"
#include "util/logging.h"
#include "elfutil.h"

void applyAllRelocations(ElfInfo* e,ElfInfo* oldElf)
{
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    char* name=elf_strptr(e->e,e->sectionHdrStrTblIdx,shdr.sh_name);
    if(!strncmp("rel.",name,strlen("rel.")))
    {
      //this is a relocations section
      Elf_Data* data=elf_getdata(scn,NULL);
      for(int j=0;j<data->d_size/shdr.sh_entsize;j++)
      {
        GElf_Rel* rel=zmalloc(sizeof(GElf_Rel));
        gelf_getrel(data,j,rel);
        List* li=zmalloc(sizeof(List));
        //todo: this is broken: wrong value type
        li->value=rel;
        if(relocsHead)
        {
          relocsTail->next=li;
        }
        else
        {
          relocsHead=li;
        }
        relocsTail=li;
      }
    }
  }
  applyRelocations(relocsHead,ON_DISK);
  deleteList(relocsHead,free);
}

addr_t getPLTEntryForSym(ElfInfo* e,int symIdx)
{
  //our procedure is as follows:
  //1. find the PLT
  //2. traverse the PLT, looking at each entry
  //   a. look up the .rel.plt entry corresponding to the reloc_offset pushed
  //   b. look up the symbol for that .rel.plt entry
  //   c. if that symbol corresponds to the symbol we're looking for, return the entry address
  //3. if we didn't find the symbol, die
  GElf_Shdr shdr;
  getShdrByERS(e,ERS_PLT,&shdr);
  addr_t pltStart=shdr.sh_addr;

  Elf_Scn* pltRelScn=getSectionByERS(e,ERS_RELX_PLT);
  if(!pltRelScn)
  {
    //todo: maybe don't die here, just return 0
    death("Executable has neither rel.plt nor rela.plt\n");
  }

#if defined(KATANA_X86_64_ARCH)
  //we'll need it later
  GElf_Shdr pltRelShdr;
  getShdr(pltRelScn,&pltRelShdr);
#endif
  
  //plt entries are aligned on 16-byte boundaries
  //we start with plt1, since plt0 has special function
  //todo: support x86_64 large code PLT model (which is much more
  //complicated and things are not separated nicely by 16 bytes)
  int i=0x10;
  for(;i<shdr.sh_size;i+=0x10)
  {
    word_t relocOffset=0;
    addr_t startWithOffset;
    //the first jmp instruction takes 6 bytes, then the
    //opcode of the push instruction takes 1 byte, so the reloc_offset
    //is after 7 bytes
    startWithOffset=pltStart+7;
    int offsetLen=4;
    #if defined(KATANA_X86_64_ARCH)
    //todo: support large-mode plt
    //in which  offsetLen will be 8
    //not supported yet because I'm not sure how to detect it best
    #endif
    
    //todo: does this need to be aligned, because it isn't
    memcpyFromTarget((byte*)&relocOffset,startWithOffset+i,offsetLen);
    logprintf(ELL_INFO_V4,ELS_RELOCATION,"relocOffset is %i\n",relocOffset);
    //now look up the appropriate relocation entry
#if defined(KATANA_X86_64_ARCH)
    //relocationOffset is actually an index, not an offset. We have to make it into
    //an offset therefore
    relocOffset*=pltRelShdr.sh_entsize;
#endif

    RelocInfo* reloc=getRelocationEntryAtOffset(e,pltRelScn,relocOffset);
    if(reloc->symIdx==symIdx)
    {
      free(reloc);
      //hooray!, we've found it
      return pltStart+i;
    }
    free(reloc);
  }
  //todo: maybe don't die here, just return
  //false or something and applyRelocation can continue as normal
  death("could not find symbol in PLT\n");
  return 0;
}

//modify the relocation entry at the given offset from the start of relocScn
void setRelocationEntryAtOffset(RelocInfo* reloc,Elf_Scn* relocScn,addr_t offset)
{
  GElf_Shdr shdr;
  if(!gelf_getshdr(relocScn,&shdr))
  {
    death("in getRelocationItemsInRange, could not get shdr for reloc section\n");
  }
  assert(SHT_REL==shdr.sh_type || SHT_RELA==shdr.sh_type);
  Elf_Data* data=elf_getdata(relocScn,NULL);
  assert(shdr.sh_size>offset);
  ElfXX_Rel rel;
  ElfXX_Rela rela;
  if(SHT_REL==shdr.sh_type)
  {
    rel.r_offset=reloc->r_offset;
    rel.r_info=ELFXX_R_INFO(reloc->symIdx,reloc->relocType);
    memcpy(data->d_buf+offset,&rel,sizeof(rel));
  }
  else //RELA
  {
    rela.r_offset=reloc->r_offset;
    rela.r_info=ELFXX_R_INFO(reloc->symIdx,reloc->relocType);
    rela.r_addend=reloc->r_addend;
    memcpy(data->d_buf+offset,&rela,sizeof(rela));
  }
}

RelocInfo* getRelocationEntryAtOffset(ElfInfo* e,Elf_Scn* relocScn,addr_t offset)
{
  GElf_Shdr shdr;
  if(!gelf_getshdr(relocScn,&shdr))
  {
    death("in getRelocationItemsInRange, could not get shdr for reloc section\n");
  }
  assert(SHT_REL==shdr.sh_type || SHT_RELA==shdr.sh_type);
  Elf_Data* data=elf_getdata(relocScn,NULL);
  assert(shdr.sh_size>offset);
  int scnIdx=shdr.sh_info;//section relocation applies to
  ElfXX_Rel rel;
  ElfXX_Rela rela;
  RelocInfo* reloc=zmalloc(sizeof(RelocInfo));
  reloc->e=e;
  reloc->scnIdx=scnIdx;
  if(SHT_REL==shdr.sh_type)
  {
    memcpy(&rel,data->d_buf+offset,sizeof(rel));
    reloc->r_offset=rel.r_offset;
    reloc->relocType=ELFXX_R_TYPE(rel.r_info);
    reloc->symIdx=ELFXX_R_SYM(rel.r_info);
    reloc->r_addend=computeAddend(e,reloc->relocType,reloc->symIdx,reloc->r_offset,reloc->scnIdx);
  }
  else //RELA
  {
    memcpy(&rela,data->d_buf+offset,sizeof(rela));
    reloc->r_offset=rela.r_offset;
    reloc->r_addend=rela.r_addend;
    reloc->relocType=ELFXX_R_TYPE(rela.r_info);
    reloc->symIdx=ELFXX_R_SYM(rela.r_info);
  }
  return reloc;
}

//apply the given relocation
void applyRelocation(RelocInfo* rel,ELF_STORAGE_TYPE type)
{
  Elf_Scn* relScn=elf_getscn(rel->e->e,rel->scnIdx);
  GElf_Shdr shdr;
  getShdr(relScn,&shdr);
  //printf("shdr.sh_flags %li, elf filename is %s\n",shdr.sh_flags,rel->e->fname);
  if(type==IN_MEM && !(shdr.sh_flags&SHF_ALLOC))
  {
    //nothing to do, this section isn't in memory
    return;
  }
    
  addr_t addrToBeRelocated=rel->r_offset;

  addr_t newAddrAccessed;
  int bytesInAddr=sizeof(addr_t);//this is needed because for x86_64, it may actually
  //be less, some relocations are only for 32 bits
  int symIdx=rel->symIdx;
  GElf_Sym sym;
  getSymbol(rel->e,symIdx,&sym);

  byte symType=ELF64_ST_TYPE(sym.st_info);
  if(sym.st_shndx==SHN_UNDEF && 0==sym.st_value && STT_FUNC==symType)
  {
    char* symName=getString(rel->e,sym.st_name);
    logprintf(ELL_INFO_V2,ELS_RELOCATION,"applying relocation at 0x%x for symbol %s\n",(uint)addrToBeRelocated,symName);
    logprintf(ELL_INFO_V1,ELS_RELOCATION,"Symbol %s should be relocated from the PLT\n",symName);
    //get the index of the symbol in the dynamic symbol table, because
    //that's what we'll be matching against when looking at the symbols
    //referenced in .rel.plt
    idx_t symIdxDynamic=reindexSymbol(rel->e,rel->e,symIdx,ESFF_FUZZY_MATCHING_OK | ESFF_NEW_DYNAMIC);

    newAddrAccessed=getPLTEntryForSym(rel->e,symIdxDynamic);
    logprintf(ELL_INFO_V2,ELS_RELOCATION,"Relocated address at 0x%x to 0x%x, after PLT lookup\n",(uint)addrToBeRelocated,newAddrAccessed);
    //may have to do some additional fixups
    byte relocType=rel->relocType;
    //use if structure rather than switch b/c of duplicate
    //case values
    if(R_386_PC32==relocType || R_X86_64_PC32==relocType)
    {
      newAddrAccessed=newAddrAccessed-rel->r_offset-4;//4 b/c 32 bits
      bytesInAddr=4;
    }
    else if(R_X86_64_PC64==relocType)
    {
      newAddrAccessed=newAddrAccessed-rel->r_offset-8;
    }
  }
  else
  {
    //regular relocation, not dealing with the PLT
    addr_t addrNew=sym.st_value;
    logprintf(ELL_INFO_V2,ELS_RELOCATION,"applying RELA relocation at 0x%x for symbol %s\n",(uint)addrToBeRelocated,getString(rel->e,sym.st_name));
    logprintf(ELL_INFO_V3,ELS_RELOCATION,"\tRelocation type is %i and addend is 0x%x\n",rel->relocType,rel->r_addend);
    if(R_386_32==rel->relocType || R_X86_64_32==rel->relocType || R_X86_64_64==rel->relocType)
    {
      logprintf(ELL_INFO_V3,ELS_RELOCATION,"\tRELA relocation at 0x%zx is straight 32 or 64 bit\n",addrToBeRelocated);
      newAddrAccessed=addrNew+rel->r_addend;
#ifdef KATANA_X86_64_ARCH
      if(R_X86_64_32==rel->relocType)
      {
        bytesInAddr=4;//not using all 8 bytes the architecture might suggest
      }
#endif
    }
    else if(R_386_PC32==rel->relocType || R_X86_64_PC32==rel->relocType)
    {
      logprintf(ELL_INFO_V3,ELS_RELOCATION,"\tRELA relocation at 0x%zx is PC32\n",addrToBeRelocated);
#ifdef KATANA_X86_64_ARCH
      bytesInAddr=4;//not using all 8 bytes the architecture might suggest
#endif
      logprintf(ELL_INFO_V4,ELS_RELOCATION,"\taddrNew: 0x%zx, r_offset: 0x%zx\n",addrNew,rel->r_offset);
      newAddrAccessed=addrNew+rel->r_addend-rel->r_offset;

    }
    else if(R_X86_64_32S==rel->relocType)
    {
      newAddrAccessed=addrNew+signExtend32To64(rel->r_addend);
    }
    else
    {
      death("relocation type %i we can't handle yet (for RELA)\n",rel->relocType);
    }

  }

  //printf("section with name %s has flags %li\n",getScnHdrString(rel->e,shdr.sh_name),shdr.sh_flags);
  if(IN_MEM & type && shdr.sh_flags & SHF_ALLOC)
  {
    logprintf(ELL_INFO_V4,ELS_RELOCATION,"Writing 0x%zx at 0x%zx\n",newAddrAccessed,addrToBeRelocated);
    memcpyToTarget(addrToBeRelocated,(byte*)&newAddrAccessed,bytesInAddr);
  }
  if(ON_DISK & type)
  {
    Elf_Data* data=elf_getdata(relScn,NULL);
    void* dataptr=data->d_buf+(addrToBeRelocated-shdr.sh_addr);
    memcpy(dataptr,&newAddrAccessed,bytesInAddr);
  }
}

//apply a list of relocations (list value type is GElf_Reloc)
void applyRelocations(List* relocs,ELF_STORAGE_TYPE type)
{
  List* li=relocs;
  for(;li;li=li->next)
  {
    RelocInfo* reloc=(RelocInfo*)li->value;
    applyRelocation(reloc,type);
  }
}


//get relocation items that live in the given relocScn
//that are for  in-memory addresses between lowAddr and highAddr inclusive
//list value type is RelocInfo
List* getRelocationItemsInRange(ElfInfo* e,Elf_Scn* relocScn,addr_t lowAddr,addr_t highAddr)
{
  assert(e);
  if(!relocScn)
  {
    return NULL;
  }

  RelocInfo* reloc=NULL;
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  GElf_Shdr shdr;
  if(!gelf_getshdr(relocScn,&shdr))
  {
    death("in getRelocationItemsInRange, could not get shdr for reloc section\n");
  }
  assert(SHT_REL==shdr.sh_type || SHT_RELA==shdr.sh_type);
  Elf_Data* data=elf_getdata(relocScn,NULL);
  int scnIdx=shdr.sh_info;//section relocation applies to
  GElf_Rel rel;
  GElf_Rela rela;
  for(int i=0;i<data->d_size/shdr.sh_entsize;i++)
  {
    if(!reloc)
    {
      reloc=zmalloc(sizeof(RelocInfo));
      reloc->e=e;
      reloc->scnIdx=scnIdx;
    }
    if(SHT_REL==shdr.sh_type)
    {
      gelf_getrel(data,i,&rel);
      if(rel.r_offset < lowAddr || rel.r_offset > highAddr)
      {
        continue;
      }
      reloc->r_offset=rel.r_offset;
      reloc->relocType=ELF64_R_TYPE(rel.r_info);//elf64 because it's GElf
      reloc->symIdx=ELF64_R_SYM(rel.r_info);//elf64 because it's GElf
      reloc->r_addend=computeAddend(e,reloc->relocType,reloc->symIdx,reloc->r_offset,reloc->scnIdx);

    }
    else //RELA
    {
      gelf_getrela(data,i,&rela);
      if(rela.r_offset < lowAddr || rela.r_offset > highAddr)
      {
        continue;
      }
      reloc->r_offset=rela.r_offset;
      reloc->r_addend=rela.r_addend;
      reloc->relocType=ELF64_R_TYPE(rela.r_info);//elf64 because it's GElf
      reloc->symIdx=ELF64_R_SYM(rela.r_info);//elf64 because it's GElf
    }
    List* li=zmalloc(sizeof(List));
    li->value=reloc;
    reloc=NULL;
    if(relocsHead)
    {
      relocsTail->next=li;
    }
    else
    {
      relocsHead=li;
    }
    relocsTail=li;
  }
  if(reloc)
  {
    free(reloc);//had an extra one allocated
  }
  return relocsHead;
}

List* getRelocationItemsFor(ElfInfo* e,int symIdx)
{
  GElf_Sym sym;
  getSymbol(e,symIdx,&sym);
  logprintf(ELL_INFO_V2,ELS_RELOCATION,"getting relocation items for symbol %s\n",getString(e,sym.st_name));
  RelocInfo* reloc=NULL;
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    GElf_Shdr shdr;
    if(!gelf_getshdr(scn,&shdr))
    {death("gelf_getshdr failed in getRelocationItemsFor\n");}
    int scnIdx=shdr.sh_info;
    if(SHT_REL==shdr.sh_type || SHT_RELA==shdr.sh_type)
    {
      //this is a relocations section
      Elf_Data* data=elf_getdata(scn,NULL);
      for(int j=0;j<data->d_size/shdr.sh_entsize;j++)
      {
        if(!reloc)
        {
          reloc=zmalloc(sizeof(RelocInfo));
          reloc->e=e;
        }
        reloc->scnIdx=scnIdx;
        if(SHT_REL==shdr.sh_type)
        {
          GElf_Rel rel;
          gelf_getrel(data,j,&rel);
          if(ELF64_R_SYM(rel.r_info)!=symIdx)
          {
            continue;
          }
          reloc->r_offset=rel.r_offset;
          reloc->relocType=ELF64_R_TYPE(rel.r_info);//elf64 because it's GElf
          reloc->symIdx=ELF64_R_SYM(rel.r_info);//elf64 because it's GElf
	  reloc->r_addend=computeAddend(e,reloc->relocType,reloc->symIdx,reloc->r_offset,reloc->scnIdx);
        }
        else //SHT_RELA
        {
          GElf_Rela rela;
          gelf_getrela(data,j,&rela);
          if(ELF64_R_SYM(rela.r_info)!=symIdx)
          {
            continue;
          }
          reloc->r_offset=rela.r_offset;
          reloc->relocType=ELF64_R_TYPE(rela.r_info);//elf64 because it's GElf
          reloc->symIdx=ELF64_R_SYM(rela.r_info);//elf64 because it's GElf
          reloc->r_addend=rela.r_addend;
        }
        List* li=zmalloc(sizeof(List));
        li->value=reloc;
        reloc=NULL;
        if(relocsHead)
        {
          relocsTail->next=li;
        }
        else
        {
          relocsHead=li;
        }
        relocsTail=li;
      }
    }
  }
  if(reloc)
  {
    free(reloc);//had an extra one allocated
  }
  return relocsHead;
}


//compute an addend for when we have REL instead of RELA
//type is relocation type
//scnIdx is section the relocation refers to
addr_t computeAddend(ElfInfo* e,byte type,idx_t symIdx,addr_t r_offset,idx_t scnIdx)
{

  if(R_386_COPY==type || R_386_GLOB_DAT==type || R_386_JMP_SLOT==type)
  {
    //addend doesn't matter for any of these, and for some off them
    //(like JMP_SLOT) r_offset may not actually be where the
    //relocation is applied, which means computation of addrAccessed
    //later could segfault, so we just return 0
    return 0;
  }
  
  GElf_Sym sym;
  getSymbol(e,symIdx,&sym);
  addr_t symVal=sym.st_value;
  if(SHN_COMMON==sym.st_shndx)
  {
    //if no space has been allocated, st_value is invalid as an address
    //and actually holds alignment constraints
    //(see http://www.sco.com/developers/gabi/latest/ch4.symtab.html#stt_common)
    //relocatable code will use 0 as the address for common symbols, so
    //we do too.
    symVal=0;
  }
  assert(sizeof(addr_t)==sizeof(word_t));
  word_t addrAccessed=getWordAtAbs(elf_getscn(e->e,scnIdx),r_offset,IN_MEM);
  switch(type)
  {
  case R_386_32: //holds for X86_X64 as well, has same numerical value
    return addrAccessed-symVal;
    break;
  case R_386_PC32: //holds for X86_X64 as well, has same numerical value
    {
      addr_t computation=addrAccessed-symVal+r_offset+sizeof(addr_t);
      //+sizeof(addr_t) because it's pc at next instruction
      if(0==symVal && computation==r_offset)
      {
        //todo: is this right? Empircally it seems to be right,
        //because setting a relative address to itself seems to be
        //GNU binutils convention for accessing something that hasn't
        //been set up yet, as the 0 for symVal also indicates,
        //and we certainly have no reason to think that any addend is called for
        //(addends are never really used with functions, except perhaps
        //with goto-style programming, need to check that out)
	
	//the above paragraph describes the rational for returning zero.
	//that didn't seem to actually be working out so well, so now we
	//return an addend which makes us address properly based on
	//the end of the instruction rather than the beginning of the relocation
	//(I think). I'm still not 100% positive this is correct,
	//but it seems similar to what gcc does with x86_64 compilation

        return -sizeof(addr_t);//necessary because address is relative to the
	//end of the instruction. This is what gcc for x86_64 does for PC32
	//relocations
      }
      return computation;
    }
    break;
  default:
    death("unhandled relocation type in computeAddend\n");
    break;
  }
  return 0;
}

//get the section containing relocations for the given function
//if want only the general relocation section, pass null for function name
//return NULL if there is no relocation section
Elf_Scn* getRelocationSection(ElfInfo* e,char* fnname)
{
 //also include any relocations which exist for anything inside this function
  //if -ffunction-sections is specified,
  //each function will have its own relocation section,
  //check this out first
  char buf[1024];
  snprintf(buf,1024,".rel.text.%s",fnname);
  Elf_Scn* relocScn=getSectionByName(e,buf);
  if(!relocScn)
  {
    snprintf(buf,1024,".rela.text.%s",fnname);
    relocScn=getSectionByName(e,buf);
  }
  if(!relocScn)
  {
    relocScn=getSectionByName(e,".rel.text");
  }
  if(!relocScn)
  {
    relocScn=getSectionByName(e,".rela.text");
  }
  if(!relocScn)
  {
    logprintf(ELL_INFO_V1,ELS_RELOCATION,"function %s in %s does not have any sort of relocation function for it\n",fnname,e->fname);
  }
  return relocScn;
}

