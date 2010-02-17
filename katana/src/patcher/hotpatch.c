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


addr_t getFreeSpaceForTransformation(TransformationInfo* trans,uint howMuch)
{
  //todo: support situation where need more than a page
  addr_t retval;
  if(howMuch<=trans->freeSpaceLeft)
  {
    retval=trans->addrFreeSpace;
  }
  else
  {
    //todo: separate out different function/structure for executable code
    retval=trans->addrFreeSpace=mmapTarget(sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE|PROT_EXEC);
    trans->freeSpaceLeft=sysconf(_SC_PAGE_SIZE);
  }
  trans->freeSpaceLeft-=howMuch;
  trans->addrFreeSpace+=howMuch;
  return retval;
  //todo: if there's a little bit of free space left,
  //we just discard it. This is wasteful
}


//takes the structure stored in datap
//of type indicated by transform->to
//and transforms it to transform->from
//and stores the result back in datap
//the original contents of datap may be freed
void fixupStructure(byte** datap,TypeTransform* transform,TransformationInfo* trans)
{
  byte* data=*datap;
  TypeInfo* oldType=transform->from;
  TypeInfo* newType=transform->to;
  byte* newData=zmalloc(newType->length);
  //now fix up the internals of the variable
  int oldOffsetSoFar=0;
  for(int i=0;i<oldType->numFields;i++)
  {
    if(FIELD_DELETED==transform->fieldOffsets[i])
    {
      continue;
    }
    printf("copying data from offset %i to offset %i\n",oldOffsetSoFar,transform->fieldOffsets[i]);
    int idxOfFieldInNew=getIdxForField(newType,oldType->fields[i]);
    TypeTransform* transformField=(TypeTransform*)dictGet(trans->typeTransformers,oldType->fieldTypes[i]->name);
    if(!transformField)
    {
      int cpLength=0;
      if(FIELD_DELETED!=idxOfFieldInNew)
      {
        cpLength=min(newType->fieldLengths[idxOfFieldInNew],oldType->fieldLengths[i]);
        if(newType->fieldLengths[idxOfFieldInNew]!=oldType->fieldLengths[i])
        {
          fprintf(stderr,"Warning: information loss: struct field changed size between versions\n");
        }
      }
      else
      {
        fprintf(stderr,"Warning: information loss: struct field was removed in patch");
      }
      memcpy(newData+transform->fieldOffsets[i],data+oldOffsetSoFar,cpLength);
    }
    else
    {
      //we can't just copy the data verbatim, its structure has changed
      byte* fieldData=zmalloc(oldType->fieldLengths[i]);
      memcpy(fieldData,data+oldOffsetSoFar,oldType->fieldLengths[i]);
      fixupStructure(&fieldData,transformField,trans);
      memcpy(newData+transform->fieldOffsets[i],fieldData,newType->fieldLengths[idxOfFieldInNew]);
      
    }
    oldOffsetSoFar+=oldType->fieldLengths[i];
  }
  free(*datap);
  *datap=newData;
}

//!!!legacy function. Do not use
//returns true if it was able to transform the variable
//todo: I don't think this will work on extern variables or
//any other sort of variable that might have multiple entries
//in the dwarf file
bool fixupVariable(VarInfo var,TransformationInfo* trans,int pid,ElfInfo* e)
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
  int idx=getSymtabIdx(e,var.name);
  long addr=getSymAddress(e,idx);//todo: deal with scope issues
  long newAddr=0;
  byte* data=NULL;
  if(newLength > oldLength)
  {
    printf("new version of type is larger than old version of type\n");
    //we'll have to allocate new space for the variable
    newAddr=getFreeSpaceForTransformation(trans,newLength);
    //now zero it out
    byte* zeros=zmalloc(newLength);
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
    byte* zeros=zmalloc(oldLength);
    newAddr=addr;
    memcpyToTarget(newAddr,zeros,newLength);
    free(zeros);
  }

  fixupStructure(&data,transform,trans);
  memcpyToTarget(newAddr,data,newLength);
  int symIdx=getSymtabIdx(e,var.name);
  //cool, the variable is all nicely relocated and whatnot. But if the variable
  //did change, we may have to relocate references to it.
  List* relocItems=getRelocationItemsFor(e,symIdx);
  for(List* li=relocItems;li;li=li->next)
  {
    GElf_Rel* rel=li->value;
    printf("relocation for %s at %x with type %i\n",var.name,(unsigned int)rel->r_offset,(unsigned int)ELF64_R_TYPE(rel->r_info));
    //modify the data to shift things over 4 bytes just to see if we can do it
    word_t oldAddrAccessed=getTextAtRelOffset(e,rel->r_offset);
    printf("old addr is ox%x\n",(uint)oldAddrAccessed);
    uint offset=oldAddrAccessed-addr;//recall, addr is the address of the symbol
    
    
    //from the offset we can figure out which field we were accessing.
    //todo: this is only a guess, not really accurate. In the final version, just rely on the code accessing the variable being recompiled
    //don't make guesses like this
    //if we aren't making those guesses we just assume the offset stays the same and relocate accordingly
    int fieldSymIdx=0;
    int offsetSoFar=0;
    for(int i=0;i<oldType->numFields;i++)
    {
      if(offsetSoFar+oldType->fieldLengths[i] > offset)
      {
        fieldSymIdx=i;
        break;
      }
      offsetSoFar+=oldType->fieldLengths[i];
    }
    printf("transformed offset %i to offset %i for variable %s\n",offset,transform->fieldOffsets[fieldSymIdx],var.name);
    offset=transform->fieldOffsets[fieldSymIdx];
    
    uint newAddrAccessed=newAddr+offset;//newAddr is new address of the symbol
    
    modifyTarget(rel->r_offset,newAddrAccessed);//now the access is fully relocated
  }
  return true;
}

void performRelocations(ElfInfo* e,VarInfo* var)
{
  int symIdx=getSymtabIdx(e,var->name);
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
    word_t oldAddrAccessed;
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
