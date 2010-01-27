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


//takes the structure stored in datap
//of type indicated by transform->to
//and transforms it to transform->from
//and stores the result back in datap
//the original contents of datap may be freed
void fixupStructure(char** datap,TypeTransform* transform,TransformationInfo* trans)
{
  char* data=*datap;
  TypeInfo* oldType=transform->from;
  TypeInfo* newType=transform->to;
  char* newData=zmalloc(newType->length);
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
    TypeTransform* transformField=(TypeTransform*)dictGet(trans->typeTransformers,oldType->fieldTypes[i]);
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
      char* fieldData=zmalloc(oldType->fieldLengths[i]);
      memcpy(fieldData,data+oldOffsetSoFar,oldType->fieldLengths[i]);
      fixupStructure(&fieldData,transformField,trans);
      memcpy(newData+transform->fieldOffsets[i],fieldData,newType->fieldLengths[idxOfFieldInNew]);
      
    }
    oldOffsetSoFar+=oldType->fieldLengths[i];
  }
  free(*datap);
  *datap=newData;
}

//returns true if it was able to transform the variable
//todo: I don't think this will work on extern variables or
//any other sort of variable that might have multiple entries
//in the dwarf file
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

  fixupStructure(&data,transform,trans);
  memcpyToTarget(newAddr,data,newLength);

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
