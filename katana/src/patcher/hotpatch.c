/*
  File: hotpatch.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: functions for actually modifying the target and performing the hotpatching
*/
#include <assert.h>
#include "target.h"
#include "elfparse.h"
#include <unistd.h>
#include "hotpatch.h"
#include "relocation.h"
#include "symbol.h"
#include <math.h>

addr_t addrFreeSpace;
addr_t freeSpaceLeft;



int getIdxForField(TypeInfo* type,char* name)
{
  for(int i=0;i<type->numFields;i++)
  {
    if(!strcmp(name,type->fields[i]))
    {
      return i;
    }
  }
  return FIELD_DELETED;
}

addr_t getFreeSpaceInTarget(uint howMuch)
{
  uint numPages=(uint)ceil((double)howMuch/(double)sysconf(_SC_PAGE_SIZE));
  //todo: support situation where need more than a page
  addr_t retval;
  if(howMuch<=freeSpaceLeft)
  {
    retval=addrFreeSpace;
  }
  else
  {
    //todo: separate out different function/structure for executable code
    retval=addrFreeSpace=mmapTarget(numPages=sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE|PROT_EXEC);
    freeSpaceLeft=sysconf(_SC_PAGE_SIZE);
  }
  freeSpaceLeft-=howMuch;
  addrFreeSpace+=howMuch;
  return retval;
  //todo: if there's a little bit of free space left,
  //we just discard it. This is wasteful
}

void performRelocations(ElfInfo* e,VarInfo* var)
{
  int symIdx=getSymtabIdx(e,var->name,0);
  addr_t addr=getSymAddress(e,symIdx);
  GElf_Sym sym;
  getSymbol(e,symIdx,&sym);
  //cool, the variable is all nicely relocated and whatnot. But if the variable
  //did change, we may have to relocate references to it.
  List* relocItems=getRelocationItemsFor(e,symIdx);
  for(List* li=relocItems;li;li=li->next)
  {
    RelocInfo* rel=li->value;
    printf("relocation for %s at %x with type %i\n",var->name,(unsigned int)rel->r_offset,(unsigned int)rel->relocType);
    word_t oldAddrAccessed=0;
    switch(rel->relocType)
    {
    case R_386_32:
      oldAddrAccessed=getTextAtAbs(e,rel->r_offset,IN_MEM);
      break;
    default:
      death("relocation type we can't handle yet\n");
    }
    printf("old addr accessed is 0x%x and symbol value is 0x%x\n",(uint)oldAddrAccessed,(uint)addr);
    uint offset=oldAddrAccessed-addr;//recall, addr is the address of the symbol
    printf("offset is %i\n",offset);

    //without precise info about the old type, can't even guess
    //which field we were accessing. That's ok though, because
    //any code accessing a changed type should have changed. Assume that
    //we've already done all the code changes. Until we have code patching
    //though, this will be wrong
    
    uint newAddrAccessed=var->newLocation+offset;//newAddr is new address of the symbol
    printf("new addr is 0x%x\n",newAddrAccessed);
    modifyTarget(rel->r_offset,newAddrAccessed);//now the access is fully relocated
  }
}
