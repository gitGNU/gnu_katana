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

#include <dwarf.h>

TypeInfo* getTypeInfoFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu);
void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool siblings);



void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg)
{
  fprintf(stderr,"Dwarf error: %s\n",dwarf_errmsg(err));
  abort();
}

//the returned pointer should be freed
char* getNameForDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  char* name=NULL;
  char* retname=NULL;
  Dwarf_Error err;
  int res=dwarf_diename(die,&name,&err);
  if(res==DW_DLV_NO_ENTRY)
  {
    //anonymous. Now it matters what type we have
    Dwarf_Half tag = 0;
    dwarf_tag(die,&tag,&err);
    Dwarf_Off offset;
    dwarf_dieoffset(die,&offset,&err);
    char buf[2048];
    switch(tag)
    {
    case DW_TAG_base_type:
      snprintf(buf,64,"banon_%i\n",(int)offset);
    case DW_TAG_member:
      snprintf(buf,64,"manon_%i\n",(int)offset);
    case DW_TAG_variable:
      snprintf(buf,64,"vanon_%i\n",(int)offset);
      break;
    case DW_TAG_structure_type:
      snprintf(buf,64,"anon_%u\n",(uint)offset);
      break;
    case DW_TAG_const_type:
      {
        TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
        if(!type)
        {
          fprintf(stderr,"the type that DW_TAG_const_type references does not exist\n");
          abort();
        }
        snprintf(buf,2048,"const %s",type->name);
      }
      break;
    case DW_TAG_pointer_type:
      {
        TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
        if(!type)
        {
          fprintf(stderr,"the type that DW_TAG_pointer_type references does not exist\n");
          abort();
        }
        snprintf(buf,2048,"%s*",type->name);
      }
      break;
    default:
      snprintf(buf,64,"unknown_type_anon_%u\n",(uint)offset);
    }
    retname=strdup(buf);
  }
  else if(DW_DLV_OK==res)
  {
    retname=strdup(name);
    dwarf_dealloc(dbg,(void*)name,DW_DLA_STRING);
  }
  else
  {
    dwarfErrorHandler(err,NULL);
  }
  return retname;
}

//gets the TypeInfo corresponding to the type
//referred to by the DW_AT_type attribute of the die in question
//returns NULL if the type doesn't exist
//or if the die doesn't have DW_AT_type
TypeInfo* getTypeInfoFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_type,&attr,&err);
  if(res!=DW_DLV_OK)
  {
    fprintf(stderr,"can't get type from die without type\n");
    return NULL;
  }
  Dwarf_Half form;
  Dwarf_Off offsetOfType;
  Dwarf_Die dieOfType;
  dwarf_whatform(attr,&form,&err);
  //the returned form should be either local or global reference to a type
  if(form==DW_FORM_ref_addr)
  {
        
    dwarf_global_formref(attr,&offsetOfType,&err);
    printf("global offset is %i\n",(int)offsetOfType);
    dwarf_offdie(dbg,offsetOfType,&dieOfType,&err);
    if(DW_DLV_ERROR==res)
    {
      dwarfErrorHandler(err,NULL);
    }

  }
  else
  {
    //printf("form is %x\n",form);
    dwarf_formref(attr,&offsetOfType,&err);
    //printf("cu-relative offset is %i\n",(int)offsetOfType);
    Dwarf_Off offsetOfCu;
    Dwarf_Off off1,off2;
    //dwarf_CU_dieoffset_given_die(die,&offsetOfCu,&err);//doesn't work for some reason
    dwarf_dieoffset(die,&off1,&err);
    dwarf_die_CU_offset(die,&off2,&err);
    offsetOfCu=off1-off2;
    //printf("offset of cu is %i\n",(int)offsetOfCu);
    int res=dwarf_offdie(dbg,offsetOfCu+offsetOfType,&dieOfType,&err);
    if(DW_DLV_ERROR==res)
    {
      dwarfErrorHandler(err,NULL);
    }
    if(DW_DLV_NO_ENTRY==res)
    {
      fprintf(stderr,"no die at offset %i\n",(int)(offsetOfCu+offsetOfType));
      abort();
    }
  }

  char* name=getNameForDie(dbg,dieOfType,cu);
  //todo: scoping? vars other than global?
  TypeInfo* result=dictGet(cu->tv->globalTypes,name);
  if(!result)
  {
    //it's possible we haven't read this die yet
    walkDieTree(dbg,dieOfType,cu,false);
  }
  result=dictGet(cu->tv->globalTypes,name);
  if(!result)
  {
    fprintf(stderr,"Type for die of name %s does not seem to exist\n",name);
  }
  return result;
}

void addBaseTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_BASE;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return;
  }
  type->length=byteSize;
  dictInsert(cu->tv->globalTypes,type->name,type);
  printf("added base type of name %s\n",type->name);
}

//read in the type definition of a structure
void addStructureFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  printf("reading structure\n");
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_STRUCT;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return;
  }
  type->length=byteSize;
  Dwarf_Die child;
  res=dwarf_child(die,&child,&err);
  if(res==DW_DLV_OK)
  {
    int idx=0;
    do
    {
      //iterate through each field of the structure
      Dwarf_Half tag = 0;
      dwarf_tag(child,&tag,&err);
      if(tag!=DW_TAG_member)
      {
        fprintf(stderr,"within structure found tag not a member, this is bizarre\n");
        continue;
      }
      type->numFields++;
      type->fields=realloc(type->fields,type->numFields);
      MALLOC_CHECK(type->fields);
      type->fieldLengths=realloc(type->fieldLengths,type->numFields);
      MALLOC_CHECK(type->fieldLengths);
      type->fieldTypes=realloc(type->fieldTypes,type->numFields);
      MALLOC_CHECK(type->fieldTypes);

      type->fields[idx]=getNameForDie(dbg,child,cu);

      TypeInfo* typeOfField=getTypeInfoFromATType(dbg,child,cu);
      if(!typeOfField)
      {
        fprintf(stderr,"error, field type doesn't seem to exist, cannot create type %s\n",type->name);
        freeTypeInfo(type);
        return;
      }
      type->fieldTypes[idx]=typeOfField->name;
      //todo: perhaps use DW_AT_location instead of this?
      type->fieldLengths[idx]=typeOfField->length;
      
      idx++;
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }
  else
  {
    fprintf(stderr,"structure %s with no members, this is odd\n",type->name);
    freeTypeInfo(type);
    return;
  }
  dictInsert(cu->tv->globalTypes,type->name,type);
  printf("added structure type %s\n",type->name);
}

void addPointerTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_POINTER;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return;
  }
  type->length=byteSize;
  TypeInfo* pointedType=getTypeInfoFromATType(dbg,die,cu);
  if(!pointedType)
  {
    fprintf(stderr,"Cannot add pointer type when the type it points to does not exist\n");
    abort();
  }
  type->pointedType=pointedType->name;
  dictInsert(cu->tv->globalTypes,type->name,type);
  printf("added pointer type of name %s\n",type->name);
}
  

void addTypedefFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  char* name=getNameForDie(dbg,die,cu);
  TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
  if(type)
  {
    dictInsert(cu->tv->globalTypes,name,type);
    printf("added typedef for name %s\n",name);
  }
  else
  {
    fprintf(stderr,"Unable to resolve typedef for %s\n",name);
  }
  free(name);
}

void addVarFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  VarInfo* var=zmalloc(sizeof(VarInfo));
  var->name=getNameForDie(dbg,die,cu);
  var->type=getTypeInfoFromATType(dbg,die,cu);
  if(!var->type)
  {
    fprintf(stderr,"Cannot create var %s when its type cannot be determined\n",var->name);
    free(var->name);
    return;
  }
  dictInsert(cu->tv->globalVars,var->name,var);
  printf("added variable %s\n",var->name);
}

void parseCompileUnit(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  cu->name=getNameForDie(dbg,die,cu);
  printf("compilation unit has name %s\n",cu->name);
}
                     
void parseDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool* parseChildren)
{
  Dwarf_Off off;
  Dwarf_Error err;
  dwarf_dieoffset(die,&off,&err);
  if(mapExists(cu->tv->parsedDies,&off))
  {
    //we've already parsed this die
    *parseChildren=false;//already will have parsed children too
    return;
  }
  *parseChildren=true;
  Dwarf_Half tag = 0;
  dwarf_tag(die,&tag,&err);
  switch(tag)
  {
  case DW_TAG_compile_unit:
    parseCompileUnit(dbg,die,cu);
    break;
  case DW_TAG_base_type:
    addBaseTypeFromDie(dbg,die,cu);
    break;
  case DW_TAG_pointer_type:
    addPointerTypeFromDie(dbg,die,cu);
    break;
  case DW_TAG_structure_type:
    addStructureFromDie(dbg,die,cu);
    *parseChildren=false;//reading the structure will have taken care of that
    break;
  case DW_TAG_typedef:
  case DW_TAG_const_type://const only changes program semantics, not memory layout, so we don't care about it really, just treat it as a typedef
    addTypedefFromDie(dbg,die,cu);
    break;
  case DW_TAG_variable:
    addVarFromDie(dbg,die,cu);
  case DW_TAG_subprogram://todo: will need to add in subprogram processing for scoping reasons
  case DW_TAG_formal_parameter: //ignore because if a function change will be recompiled and won't modify function while executing it
    break;
  default:
    fprintf(stderr,"Unknown die type 0x%x. Ignoring it but this may not be what you want\n",tag);
    
  }
  //map requires memory allocated for the key
  int* key=zmalloc(sizeof(int));
  *key=off;
  //insert the key as its value too because all we
  //care about is its existance, not its value
  mapInsert(cu->tv->parsedDies,key,key);
}

 void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool siblings)
{
  //code inspired by David Anderson's simplereader.c
  //distributed with libdwarf
  char* name=NULL;
  Dwarf_Error err=0;
  dwarf_diename(die,&name,&err);
  if(name)
  {
    //printf("found die with name %s\n",name);
  }
  else
  {
    //printf("found die with no name\n");
  }
  dwarf_dealloc(dbg,name,DW_DLA_STRING);
  bool parseChildren=true;
  parseDie(dbg,die,cu,&parseChildren);
  Dwarf_Die childOrSibling;
  if(parseChildren)
  {
    
    int res=dwarf_child(die,&childOrSibling,&err);
    if(res==DW_DLV_OK)
    {
      //if result was error our callback will have been called
      //if however there simply is no child it won't be an error
      //but the return value won't be ok
      //printf("walking child\n");
      walkDieTree(dbg,childOrSibling,cu,true);
      dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);
    }
  }
  if(!siblings)
  {
    return;
  }
  int res=dwarf_siblingof(dbg,die,&childOrSibling,&err);
  if(res==DW_DLV_ERROR)
  {
    dwarfErrorHandler(err,NULL);
  }
  if(res==DW_DLV_NO_ENTRY)
  {
    //no sibling
    //printf("no sibling\n");
    return;
  }
  //printf("walking sibling\n");
  walkDieTree(dbg,childOrSibling,cu,true);
  dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);

}

//the returned structure should be freed
//when the caller is finished with it
DwarfInfo* readDWARFTypes(Elf* elf)
{
  DwarfInfo* di=zmalloc(sizeof(DwarfInfo));
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
    CompilationUnit* cu=zmalloc(sizeof(CompilationUnit));
    List* cuLi=zmalloc(sizeof(List));
    cuLi->value=cu;
    if(di->compilationUnits)
    {
      di->lastCompilationUnit->next=cuLi;
    }
    else
    {
      di->compilationUnits=cuLi;
    }
    di->lastCompilationUnit=cuLi;
          
    TypeAndVarInfo* tv=zmalloc(sizeof(TypeAndVarInfo));
    cu->tv=tv;
    tv->globalTypes=dictCreate(100);//todo: get rid of magic number 100 and base it on smth
    tv->globalVars=dictCreate(100);//todo: get rid of magic number 100 and base it on smth
    tv->parsedDies=integerMapCreate(100);//todo: get rid of magic number 100 and base it on smth
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
    walkDieTree(dbg,cu_die,cu,true);
    dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
  }
  
  if(DW_DLV_OK!=dwarf_finish(dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  return di;
}


