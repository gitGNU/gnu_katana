/*
  File: katana-prelim.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: preliminary test functions for katana. They don't need to
               be included in the final version
*/


//test relocation of the variable bar
//knowing some very specific things about it
void testManualRelocateBar()
{
  int barSymIdx=getSymtabIdx("bar");
  int barAddress=getSymAddress(barSymIdx);
  logprintf(ELL_INFO_V1,ELS_MISC,"bar located at %x\n",(unsigned int)barAddress);

  List* relocItems=getRelocationItemsFor(barSymIdx);
  //allocate a new page for us to put the new variable in
  long newPageAddr=mmapTarget(sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE);
  logprintf(ELL_INFO_V1,ELS_MISC,"mapped in a new page at 0x%x\n",(uint)newPageAddr);

  //now set the data in that page
  //first get it from the old address
  uint barData[4];
  memcpyFromTarget((char*)barData,barAddress,sizeof(int)*3);
  logprintf(ELL_INFO_V1,ELS_MISC,"read data %i,%i,%i\n",barData[0],barData[1],barData[2]);
  //copy it to the new address
  memcpyToTarget(newPageAddr,(char*)barData,sizeof(int)*3);

  //now test to make sure we copied it correctly
  memcpyFromTarget((char*)barData,newPageAddr,sizeof(int)*3);
  logprintf(ELL_INFO_V1,ELS_MISC,"read new data %i,%i,%i\n",barData[0],barData[1],barData[2]);
  
  for(List* li=relocItems;li;li=li->next)
  {
    GElf_Rel* rel=li->value;
    logprintf(ELL_INFO_V1,ELS_MISC,"relocation for bar at %x with type %i\n",(unsigned int)rel->r_offset,(unsigned int)ELF64_R_TYPE(rel->r_info));
    addr_t oldAddr=getTextAtRelOffset(rel->r_offset);
    logprintf(ELL_INFO_V1,ELS_MISC,"old addr is 0x%x\n",(uint)oldAddr);
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
