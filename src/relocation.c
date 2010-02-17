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
  int symIdx=rel->symIdx;
  addr_t addrNew=getSymAddress(rel->e,symIdx);
  if(ERT_REL==rel->type)
  {
    addr_t addrOld=oldSym->st_value;
    addr_t oldAddrAccessed;
    switch(rel->relocType)
    {
    case R_386_32:
      //todo: we're assuming we're in the text section
      oldAddrAccessed=getTextAtAbs(rel->e,rel->r_offset,IN_MEM);
      break;
    default:
      death("relocation type we can't handle yet (for REL)\n");
    }
    uint offset=oldAddrAccessed-addrOld;
    newAddrAccessed=addrNew+offset;
    addrToBeRelocated=rel->r_offset;
  }
  else //RELA
  {
    addrToBeRelocated=rel->r_offset;
    printf("applying relocation at 0x%x\n",(uint)addrToBeRelocated);
    switch(rel->relocType)
    {
    case R_386_32:
      newAddrAccessed=addrNew+rel->r_addend;
      break;
    case R_386_PC32:
      //off by one?
      //printf("addrNew is 0x%x, addend ix 0x%x, offset is 0x%x\n",addrNew,rel->rela.r_addend,rel->rela.r_offset);
      newAddrAccessed=addrNew+rel->r_addend-rel->r_offset;
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
    int symIdx=reloc->symIdx;
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
  GElf_Rel rel;
  GElf_Rela rela;
  for(int i=0;i<data->d_size/shdr.sh_entsize;i++)
  {
    if(!reloc)
    {
      reloc=zmalloc(sizeof(RelocInfo));
      reloc->e=e;
      reloc->scnIdx=scnIdx;
      reloc->type=type;
    }
    if(ERT_REL==type)
    {
      gelf_getrel(data,i,&rel);
      if(rel.r_offset < lowAddr || rel.r_offset > highAddr)
      {
        continue;
      }
      reloc->r_offset=rel.r_offset;
      reloc->relocType=ELF64_R_TYPE(rel.r_info);//elf64 because it's GElf
      reloc->symIdx=ELF64_R_SYM(rel.r_info);//elf64 because it's GElf
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
  //note: system V architecture only uses REL entries,
  //as long as we're sticking with Linux shouldn't
  //have to worry about RELA
  //todo:make sure this is what we want, since our patch objects use rela sections
  RelocInfo* reloc=NULL;
  List* relocsHead=NULL;
  List* relocsTail=NULL;
  GElf_Rel rel;
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
        gelf_getrel(data,j,&rel);
        //printf("found relocation item for symbol %u\n",(unsigned int)ELF64_R_SYM(rel.r_info));
        if(ELF64_R_SYM(rel.r_info)!=symIdx)
        {
          continue;
        }
        reloc->r_offset=rel.r_offset;
        reloc->relocType=ELF64_R_TYPE(rel.r_info);//elf64 because it's GElf
        reloc->symIdx=ELF64_R_SYM(rel.r_info);//elf64 because it's GElf
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

//if the reloc has an addend, return it, otherwise compute it
addr_t getAddendForReloc(RelocInfo* reloc)
{
  if(ERT_RELA==reloc->type)
  {
    return reloc->r_addend;
  }
  return computeAddend(reloc->e,reloc->relocType,reloc->symIdx,reloc->r_offset);
}
