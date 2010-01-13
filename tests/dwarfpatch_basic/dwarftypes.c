/*
  File: dwarftypes.c
  Author: James Oakley
  Project: Katana (Preliminary Work)
  Date: January 10
  Description: functions for reading/manipulating DWARF type information in an ELF file
*/


#include "dictionary.h"
#include <libdwarf.h>
#include <stdlib.h>
#include <libelf.h>
#include <stdbool.h>
#include "types.h"
#include "target.h"
#include "elfparse.h"

void getFreeSpaceForTransformation(TransformationInfo* trans,uint howMuch)
{
  if(howMuch<=trans->freeSpaceLeft)
  {
    return;
  }
  trans->addrFreeSpace=mmapTarget(sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE);
  trans->freeSpaceLeft=sysconf(_SC_PAGE_SIZE);
  //todo: if there's a little bit of free space left,
  //we just discard it. This is wasteful
}

//return false if the two types are not
//identical in all regards
//if the types are not identical, return
//transformation information necessary
//to convert from type a to type b
//todo: add in this transformation info
bool compareTypes(TypeInfo* a,TypeInfo* b)
{
  if(strcmp(a->name,b->name) ||
     a->numFields!=b->numFields)
  {
    return false;
  }
  for(int i=0;i<a->numFields;i++)
  {
    //todo: do we need to update if just the name changes?
    //certainly won't need to relocate
    if(a->fieldLengths[i]!=b->fieldLengths[i] ||
       strcmp(a->fieldTypes[i],b->fieldTypes[i]))
    {
      return false;
    }
    //todo: what if a field stays the same type, but that type changes
    //need to be able to support that
  }
  return true;
}

//returns true if it was able to transform the variable
bool fixupVariable(VarInfo var,TransformationInfo* trans,int pid)
{
  TypeTransform* transform=(TypeTransform*)dictGet(trans->typeTransformers,var.type->name);
  if(!transform)
  {
    fprintf(stderr,"the transformation information for that variable doesn't exist\n");
    return false;
  }
  int newLength=transform->to->length;
  int oldLength=transform->from->length;
  TypeInfo* oldType=transform->from;
  //first we have to get the current value of the variable
  int idx=getSymtabIdx(var.name);
  long addr=getSymAddress(idx);//todo: deal with scope issues
  long newAddr=0;
  char* data=NULL;
  if(newLength > oldLength)
  {
    printf("new version of type is larger than old version of type\n");
    //we'll have to allocate new space for the variable
    getFreeSpaceForTransformation(trans,newLength);
    newAddr=trans->addrFreeSpace;
    trans->addrFreeSpace+=newLength;
    trans->freeSpaceLeft-=newLength;
    //now zero it out
    char* zeros=zmalloc(newLength);
    memcpyToTarget(newAddr,zeros,newLength);
    free(zeros);
    //now put in the fields that we know about
    data=malloc(oldLength);
    MALLOC_CHECK(data);
    memcpyFromTarget(data,addr,oldLength);

  }
  else
  {
    printf("new version of type is not larger than old version of type\n");
    
    //we can put the variable in the same space (possibly wasting a little)
    //we just have to copy it all out first and then zero the space out before
    //copying things in (opposite order of above case)
    
    
    //put in the fields that we know about
    data=malloc(oldLength);
    MALLOC_CHECK(data);
    memcpyFromTarget(data,addr,oldLength);
    char* zeros=zmalloc(oldLength);
    newAddr=addr;
    memcpyToTarget(newAddr,zeros,newLength);
    free(zeros);
  }
  int oldOffsetSoFar=0;
  for(int i=0;i<oldType->numFields;i++)
  {
    if(FIELD_DELETED==transform->fieldOffsets[i])
    {
      continue;
    }
    printf("copying data from offset %i to offset %i\n",oldOffsetSoFar,transform->fieldOffsets[i]);
    memcpyToTarget(newAddr+transform->fieldOffsets[i],data+oldOffsetSoFar,oldType->fieldLengths[i]);
    oldOffsetSoFar+=oldType->fieldLengths[i];
  }

  //cool, the variable is all nicely relocated and whatnot. But if the variable
  //did change, we may have to relocate references to it.
  List* relocItems=getRelocationItemsFor(idx);
  for(List* li=relocItems;li;li=li->next)
  {
    GElf_Rel* rel=li->value;
    printf("relocation for %s at %x with type %i\n",var.name,(unsigned int)rel->r_offset,(unsigned int)ELF64_R_TYPE(rel->r_info));
    //modify the data to shift things over 4 bytes just to see if we can do it
    word_t oldAddrAccessed=getTextAtRelOffset(rel->r_offset);
    printf("old addr is ox%x\n",(uint)oldAddrAccessed);
    uint offset=oldAddrAccessed-addr;//recall, addr is the address of the symbol
    
    
    //from the offset we can figure out which field we were accessing.
    //todo: this is only a guess, not really accurate. In the final version, just rely on the code accessing the variable being recompiled
    //don't make guesses like this
    //if we aren't making those guesses we just assume the offset stays the same and relocate accordingly
    int fieldIdx=0;
    int offsetSoFar=0;
    for(int i=0;i<oldType->numFields;i++)
    {
      if(offsetSoFar+oldType->fieldLengths[i] > offset)
      {
        fieldIdx=i;
        break;
      }
      offsetSoFar+=oldType->fieldLengths[i];
    }
    printf("transformed offset %i to offset %i for variable %s\n",offset,transform->fieldOffsets[fieldIdx],var.name);
    offset=transform->fieldOffsets[fieldIdx];
    
    uint newAddrAccessed=newAddr+offset;//newAddr is new address of the symbol
    
    modifyTarget(rel->r_offset,newAddrAccessed);//now the access is fully relocated
  }
  return true;
}
  

void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg)
{
  fprintf(stderr,"Dwarf error\n");
  exit(1);
}

void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die)
{
  //code inspired by David Anderson's simplereader.c
  //distributed with libdwarf
  char* name=NULL;
  Dwarf_Error err=0;
  dwarf_diename(die,&name,&err);
  printf("found die with name %s\n",name);
  dwarf_dealloc(dbg,name,DW_DLA_STRING);
  Dwarf_Die childOrSibling;
  int res=dwarf_child(die,&childOrSibling,&err);
  if(res==DW_DLV_OK)
  {
    //if result was error our callback will have been called
    //if however there simply is no child it won't be an error
    //but the return value won't be ok
    walkDieTree(dbg,childOrSibling);
    dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);
  }
  res=dwarf_siblingof(dbg,die,&childOrSibling,&err);
  if(res==DW_DLV_ERROR)
  {
    dwarfErrorHandler(err,NULL);
  }
  if(res==DW_DLV_NO_ENTRY)
  {
    //no sibling
    return;
  }
  walkDieTree(dbg,childOrSibling);
  dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);

}

void readDWARFTypes(Elf* elf)
{
  Dwarf_Error err;
  Dwarf_Debug dbg;
  if(DW_DLV_OK!=dwarf_elf_init(elf,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  //code inspired by David Anderson's simplereader.c
  //distributed with libdwarf
  Dwarf_Unsigned nextCUHeader=0;//todo: does this need to be initialized outside
                                //the loop, doees dwarf_next_cu_header read it,
                                //or does it keep its own state and only set it?
  while(1)
  {
    printf("iterating compilation units\n");
    Dwarf_Unsigned cuHeaderLength=0;
    Dwarf_Half version=0;
    Dwarf_Unsigned abbrevOffset=0;
    Dwarf_Half addressSize=0;
    Dwarf_Die cu_die = 0;
    int res;
    res = dwarf_next_cu_header(dbg,&cuHeaderLength,&version, &abbrevOffset,
                               &addressSize,&nextCUHeader, &err);
    if(res == DW_DLV_ERROR)
    {
      dwarfErrorHandler(err,NULL);
    }
    if(res == DW_DLV_NO_ENTRY)
    {
      //finished reading all compilation units
      break;
    }
    // The CU will have a single sibling, a cu_die.
    //passing NULL gets the first die in the CU
    if(DW_DLV_ERROR==dwarf_siblingof(dbg,NULL,&cu_die,&err))
    {
      dwarfErrorHandler(err,NULL);
    }
    if(res == DW_DLV_NO_ENTRY)
    {
      fprintf(stderr,"no entry! in dwarf_siblingof on CU die. This should never happen. Something is terribly wrong \n");
      exit(1);
    }
    walkDieTree(dbg,cu_die);
    dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
  }
  
  if(DW_DLV_OK!=dwarf_finish(dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
}


