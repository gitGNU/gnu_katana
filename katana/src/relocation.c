/*
  File: relocation.h
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description:  methods to deal with relocation and by association with some symbol issues
*/

#include "relocation.h"
#include "util/util.h"
#include "patcher/target.h"
#include <assert.h>
#include "symbol.h"


/*
//apply all relocations corresponding to the movement of certain symbols
//The List parameter should be a List with values of type
//SymMoveInfo. Relocations are applied into the newElf fields
//in the SymMoveInfo structs
void applyRelocationsForSymbols(List* symMoves);
{
  List* li=symMoves;
  for(;li;li=li->next)
  {
    SymMoveInfo* symMove=(SymMoveInfo*)li->value;
    List* moreRelocs=getRelocationItemsFor(symMove->newElf,);
    relocsHead=concatLists(relocsHead,relocsTail,moreRelocs,NULL,&relocsTail);
  }
  }*/

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
  applyRelocations(relocsHead,oldElf);
  deleteList(relocsHead,free);
}

//apply the given relocation using oldSym for reference
//to calculate the offset from the symol address
//oldSym may be NULL if the relocation type is ERT_RELA instead of ERT_REL
void applyRelocation(RelocInfo* rel,GElf_Sym* oldSym,ELF_STORAGE_TYPE type)
{
  addr_t addrToBeRelocated;
  addr_t newAddrAccessed;
  int symIdx;
  if(ERT_REL==rel->type)
  {
    symIdx=ELF64_R_SYM(rel->rel.r_info);//elf64 b/c using GElf
  }
  else
  {
    symIdx=ELF64_R_SYM(rel->rela.r_info);//elf64 b/c using GElf
  }
  addr_t addrNew=getSymAddress(rel->e,symIdx);
  if(ERT_REL==rel->type)
  {
    addr_t addrOld=oldSym->st_value;
    addr_t oldAddrAccessed;
    switch(ELF64_R_TYPE(rel->rel.r_info))
    {
    case R_386_32:
      //todo: we're assuming we're in the text section
      oldAddrAccessed=getTextAtAbs(rel->e,rel->rel.r_offset,IN_MEM);
      break;
    default:
      death("relocation type we can't handle yet (for REL)\n");
    }
    uint offset=oldAddrAccessed-addrOld;
    newAddrAccessed=addrNew+offset;
    addrToBeRelocated=rel->rel.r_offset;
  }
  else //RELA
  {
    addrToBeRelocated=rel->rela.r_offset;
    int type=ELF64_R_TYPE(rel->rela.r_info);
    switch(type)
    {
    case R_386_32:
      newAddrAccessed=addrNew+rel->rela.r_addend;
      break;
    case R_386_PC32:
      //off by one?
      newAddrAccessed=addrNew+rel->rela.r_addend-rel->rela.r_offset;
      break;
    default:
      death("relocation type %i we can't handle yet (for RELA)\n",type);
    }
  }
  
  if(IN_MEM & type)
  {
    modifyTarget(addrToBeRelocated,newAddrAccessed);
  }
  if(ON_DISK & type)
  {
    Elf_Scn* scn=elf_getscn(rel->e->e,rel->scnIdx);
    Elf_Data* data=elf_getdata(scn,NULL);
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    void* dataptr=data->d_buf+(addrToBeRelocated-shdr.sh_addr);
    memcpy(dataptr,&newAddrAccessed,sizeof(addr_t));
  }
}

//apply a list of relocations (list value type is GElf_Reloc)
//oldElf is the elf object containing the symbol information
//that items were originally located against. This is necessary
//to compute the offsets from symbols
void applyRelocations(List* relocs,ElfInfo* oldElf)
{
  List* li=relocs;
  for(;li;li=li->next)
  {
    RelocInfo* reloc=(RelocInfo*)li->value;
    int symIdx=ELF64_R_SYM(reloc->rel.r_info);//elf64 b/c using GElf
    int oldSymIdx=reindexSymbol(reloc->e,oldElf,symIdx);
    if(oldSymIdx>0)
    {
      GElf_Sym oldSym;
      getSymbol(oldElf,oldSymIdx,&oldSym);
      applyRelocation(reloc,&oldSym,ON_DISK);//todo: is this right?
    }
    else
    {
      death("in apply relocations, unable to reindex symbol between the two elf files\n");
    }
  }
}


//get relocation items that live in the given relocScn
//that are for  in-memory addresses between lowAddr and highAddr inclusive
//list value type is RelocInfo
List* getRelocationItemsInRange(ElfInfo* e,Elf_Scn* relocScn,addr_t lowAddr,addr_t highAddr)
{
  RelocInfo* reloc=NULL;
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  GElf_Shdr shdr;
  gelf_getshdr(relocScn,&shdr);
  assert(SHT_REL==shdr.sh_type || SHT_RELA==shdr.sh_type);
  E_RELOC_TYPE type=SHT_REL==shdr.sh_type?ERT_REL:ERT_RELA;
  Elf_Data* data=elf_getdata(relocScn,NULL);
  int scnIdx=elf_ndxscn(relocScn);
  for(int i=0;i<data->d_size/shdr.sh_entsize;i++)
  {
    if(!reloc)
    {
      reloc=zmalloc(sizeof(RelocInfo));
      reloc->e=e;
      reloc->scnIdx=scnIdx;
      reloc->type=type;
    }
    if(ERT_REL==reloc->type)
    {
      gelf_getrel(data,i,&reloc->rel);
      if(reloc->rel.r_offset < lowAddr || reloc->rel.r_offset > highAddr)
      {
        continue;
      }
    }
    else //RELA
    {
      gelf_getrela(data,i,&reloc->rela);
      if(reloc->rela.r_offset < lowAddr || reloc->rela.r_offset > highAddr)
      {
        continue;
      }
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
  //note: system V architecture only uses REL entries,
  //as long as we're sticking with Linux shouldn't
  //have to worry about RELA
  RelocInfo* reloc=NULL;
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    int scnIdx=elf_ndxscn(scn);
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    char* name=elf_strptr(e->e,e->sectionHdrStrTblIdx,shdr.sh_name);
    if(!strncmp("rel.",name,strlen("rel.")))
    {
      //this is a relocations section
      Elf_Data* data=elf_getdata(scn,NULL);
      for(int j=0;j<data->d_size/shdr.sh_entsize;j++)
      {
        if(!reloc)
        {
          reloc=zmalloc(sizeof(RelocInfo));
          reloc->e=e;
          reloc->scnIdx=scnIdx;
        }
        gelf_getrel(data,j,&reloc->rel);
        //printf("found relocation item for symbol %u\n",(unsigned int)ELF64_R_SYM(rel.r_info));
        if(ELF64_R_SYM(reloc->rel.r_info)!=symIdx)
        {
          continue;
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
addr_t computeAddend(ElfInfo* e,byte type,idx_t symIdx,addr_t r_offset)
{
  addr_t symVal=getSymAddress(e,symIdx);
  word_t addrAccessed=getTextAtAbs(e,r_offset,IN_MEM);
  switch(type)
  {
  case R_386_32:
    return addrAccessed-symVal;
    break;
  case R_386_PC32:

    return addrAccessed-symVal+r_offset+1;
    //+1 because it's pc at next instruction
    break;
  default:
    death("unhandled relocation type in computeAddend\n");
    break;
  }
  return 0;
}
