/*
  File: katana-prelim.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: January 2010
  Description: preliminary test functions for katana. They don't need to
               be included in the final version
*/

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

//test relocation of the variable bar
//knowing some very specific things about it
void testManualRelocateBar()
{
  int barSymIdx=getSymtabIdx("bar");
  int barAddress=getSymAddress(barSymIdx);
  logprintf(ELL_INFO_V4,ELS_MISC,"bar located at %x\n",(unsigned int)barAddress);

  List* relocItems=getRelocationItemsFor(barSymIdx);
  //allocate a new page for us to put the new variable in
  long newPageAddr=mmapTarget(sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE);
  logprintf(ELL_INFO_V4,ELS_MISC,"mapped in a new page at 0x%x\n",(uint)newPageAddr);

  //now set the data in that page
  //first get it from the old address
  uint barData[4];
  memcpyFromTarget((char*)barData,barAddress,sizeof(int)*3);
  logprintf(ELL_INFO_V4,ELS_MISC,"read data %i,%i,%i\n",barData[0],barData[1],barData[2]);
  //copy it to the new address
  memcpyToTarget(newPageAddr,(char*)barData,sizeof(int)*3);

  //now test to make sure we copied it correctly
  memcpyFromTarget((char*)barData,newPageAddr,sizeof(int)*3);
  logprintf(ELL_INFO_V4,ELS_MISC,"read new data %i,%i,%i\n",barData[0],barData[1],barData[2]);
  
  for(List* li=relocItems;li;li=li->next)
  {
    GElf_Rel* rel=li->value;
    logprintf(ELL_INFO_V4,ELS_MISC,"relocation for bar at %x with type %i\n",(unsigned int)rel->r_offset,(unsigned int)ELF64_R_TYPE(rel->r_info));
    addr_t oldAddr=getTextAtRelOffset(rel->r_offset);
    logprintf(ELL_INFO_V4,ELS_MISC,"old addr is 0x%x\n",(uint)oldAddr);
    uint newAddr=newPageAddr+(oldAddr-barAddress);
    //*oldAddr=newAddr;
    modifyTarget(rel->r_offset,newAddr);
  }
}

//again test relocation of the variable bar using specific
//knowledge we have about the source program, but this
//time use the fixupVariable function and the type transformation
//structures that will be automatically created in the future.
void testManualRelocateAndTransformBar()
{
  TypeInfo fooType;
  fooType.name=strdup("Foo");
  fooType.length=12;
  fooType.numFields=3;
  fooType.fields=zmalloc(sizeof(char*)*3);
  fooType.fieldLengths=zmalloc(sizeof(int)*3);
  fooType.fieldTypes=zmalloc(sizeof(char*)*3);
  fooType.fields[0]="field1";
  fooType.fields[1]="field2";
  fooType.fields[2]="field3";
  fooType.fieldLengths[0]=fooType.fieldLengths[1]=fooType.fieldLengths[2]=sizeof(int);
  fooType.fieldTypes[0]=strdup("int");
  fooType.fieldTypes[1]=strdup("int");
  fooType.fieldTypes[2]=strdup("int");
  TypeInfo fooType2;//version with an extra field
  fooType2.name=strdup("Foo");
  fooType2.length=16;
  fooType2.numFields=3;
  fooType2.fields=zmalloc(sizeof(char*)*4);
  fooType2.fieldLengths=zmalloc(sizeof(int)*4);
  fooType2.fieldTypes=zmalloc(sizeof(char*)*4);
  fooType2.fields[0]="field1";
  fooType2.fields[1]="field_extra";
  fooType2.fields[2]="field2";
  fooType2.fields[3]="field3";
  fooType2.fieldLengths[0]=fooType2.fieldLengths[1]=fooType2.fieldLengths[2]=fooType2.fieldLengths[3]=sizeof(int);
  fooType2.fieldTypes[0]=strdup("int");
  fooType2.fieldTypes[1]=strdup("int");
  fooType2.fieldTypes[2]=strdup("int");
  fooType2.fieldTypes[3]=strdup("int");
  VarInfo barInfo;
  barInfo.name="bar";
  barInfo.type=&fooType;
  TypeTransform trans;
  trans.from=&fooType;
  trans.to=&fooType2;
  trans.fieldOffsets=zmalloc(sizeof(int)*3);
  trans.fieldOffsets[0]=0;
  trans.fieldOffsets[1]=8;
  trans.fieldOffsets[2]=12;
  TransformationInfo transInfo;
  transInfo.typeTransformers=dictCreate(10);//todo: 10 a magic number
  dictInsert(transInfo.typeTransformers,barInfo.type->name,&trans);
  transInfo.varsToTransform=NULL;//don't actually need it for fixupVariable
  transInfo.freeSpaceLeft=0;
  fixupVariable(barInfo,&transInfo,pid);
  //todo: free memory
}
