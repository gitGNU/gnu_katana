/*
  File: dwarftypes.c
  Author: James Oakley
  Project: Katana (Preliminary Work)
  Date: January 10
  Description: functions for reading/manipulating DWARF type information in an ELF file
*/


#include <stdlib.h>
#include <libelf.h>
#include <stdbool.h>
#include "types.h"
#include <assert.h>
#include <dwarf.h>
#include "elfparse.h"
#include "util/logging.h"

Dictionary* cuIdentifiers=NULL;
DwarfInfo* di;



TypeInfo* getTypeInfoFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu);
char* getTypeNameFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,Dwarf_Die* dieOfType);

void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool siblings);


void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg)
{
  if(arg)
  {
    fprintf(stderr,"%s\n",(char*)arg);
  }
  fprintf(stderr,"Dwarf error: %s\n",dwarf_errmsg(err));
  fflush(stderr);
  abort();
}

int readAttributeAsInt(Dwarf_Attribute attr)
{
  Dwarf_Half form;
  Dwarf_Error err;
  int result=0;
  Dwarf_Unsigned data;
  dwarf_whatform(attr,&form,&err);
  switch(form)
  {
  case DW_FORM_data1:
  case DW_FORM_data2:
  case DW_FORM_data4:
    //todo: deal with sign issues
    //might be unsigned
    dwarf_formudata(attr,&data,&err);
    result=(int)data;
    break;
  case DW_FORM_flag:
    {
      Dwarf_Bool b;
      dwarf_formflag(attr,&b,&err);
      result=b?1:0;
    }
    break;
  case DW_FORM_addr:
    {
      Dwarf_Addr addr;
      dwarf_formaddr(attr,&addr,&err);
      assert(sizeof(addr_t)==sizeof(int));
      result=(int)addr;
    }
    break;
  default:
    fprintf(stderr,"readAttributeAsInt cannot handle form type 0x%x yet\n",form);
    death(NULL);
  }
  return result;
}

//returned string should be freed when you're finished with it
char* readAttributeAsString(Dwarf_Attribute attr)
{
  Dwarf_Half form;
  Dwarf_Error err;
  char* result="";
  char* data;
  dwarf_whatform(attr,&form,&err);
  switch(form)
  {
  case DW_FORM_string:
    //todo: deal with sign issues
    //might be unsigned
    dwarf_formstring(attr,&data,&err);
    result=strdup(data);
    //todo: don't have dbg accessible here, mem leak
    //dwarf_dealloc(dbg,DW_DLA_STRING,data,&err);
    break;
  default:
    fprintf(stderr,"readAttributeAsString cannot handle form type 0x%x yet\n",form);
    death(NULL);
  }
  return result;
}

//die must be the die for the cu
void setIdentifierForCU(CompilationUnit* cu,Dwarf_Die die)
{
  if(dictExists(cuIdentifiers,cu->name))
  {
    //ok, we have multiple compilation units with the same name, need to use the compilation path as well
      Dwarf_Attribute attr;
      Dwarf_Error err;
      dwarf_attr(die,DW_AT_comp_dir,&attr,&err);
      char* dir=readAttributeAsString(attr);
      cu->id=malloc(strlen(cu->name)+strlen(dir)+3);
      sprintf(cu->id,"%s:%s",cu->name,dir);
      free(dir);
  }
  else
  {
    cu->id=strdup(cu->name);
  }
  //don't actually care what value we insert, just making
  //a record that there's something
  dictInsert(cuIdentifiers,cu->id,cu->id);
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
        char* name=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!name)
        {
          fprintf(stderr,"the type that DW_TAG_const_type references does not exist\n");
          fflush(stderr);
          abort();
        }
        snprintf(buf,2048,"const %s",name);
      }
      break;
    case DW_TAG_pointer_type:
      {
        char* name=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!name)
        {
          fprintf(stderr,"WARNING: the type that DW_TAG_pointer_type references does not exist\n");
          snprintf(buf,2048,"pgeneric%i*",(int)offset);
        }
        else
        {
          snprintf(buf,2048,"%s*",name);
        }
      }
      break;
    case DW_TAG_array_type:
      {
        char* name=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!name)
        {
          death("the type that DW_TAG_array_type references does not exist\n");
        }
        //offset is still needed to make the name unique as arrays
        //of the same type but different bounds will come out differently in
        //the dwarf type information
        snprintf(buf,2048,"%s[]_%i",name,(int)offset);
      }
      break;
    case DW_TAG_subprogram:
      snprintf(buf,64,"lambda_%i\n",(int)offset);
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

//the main difference between this function
//and getTypeInfoFromATType aside from the return type
//is that this one respects typedefs, at least partially
//optional dieOfType argument is set to the die of the discovered type
//assuming it's found (i.e. will not be set if there was no DW_AT_type attribute)
char* getTypeNameFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,Dwarf_Die* dieOfType)
{
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_type,&attr,&err);
  if(res==DW_DLV_NO_ENTRY)
  {
    logprintf(ELL_INFO_V4,ELS_MISC ,"no type entry\n");
    //this happens in gcc-generated stuff, there's no DWARF entry
    //for the void type, so we have to create it manually
    return "void";
  }
  Dwarf_Half form;
  Dwarf_Off offsetOfType;
  bool shouldFreeDieOfType=false;
  if(!dieOfType)
  {
    dieOfType=zmalloc(sizeof(Dwarf_Die));
    shouldFreeDieOfType=true;
  }
  dwarf_whatform(attr,&form,&err);
  //the returned form should be either local or global reference to a type
  if(form==DW_FORM_ref_addr)
  {
    dwarf_global_formref(attr,&offsetOfType,&err);
    //logprintf(ELL_INFO_V4,ELS_MISC,"global offset is %i\n",(int)offsetOfType);
    dwarf_offdie(dbg,offsetOfType,dieOfType,&err);
    if(DW_DLV_ERROR==res)
    {
      dwarfErrorHandler(err,NULL);
    }
  }
  else
  {
    //logprintf(ELL_INFO_V4,ELS_MISC,"form is %x\n",form);
    dwarf_formref(attr,&offsetOfType,&err);
    //logprintf(ELL_INFO_V4,ELS_MISC,"cu-relative offset is %i\n",(int)offsetOfType);
    Dwarf_Off offsetOfCu;
    Dwarf_Off off1,off2;
    //dwarf_CU_dieoffset_given_die(die,&offsetOfCu,&err);//doesn't work for some reason
    dwarf_dieoffset(die,&off1,&err);
    dwarf_die_CU_offset(die,&off2,&err);
    offsetOfCu=off1-off2;
    //logprintf(ELL_INFO_V4,ELS_MISC,"offset of cu is %i\n",(int)offsetOfCu);
    int res=dwarf_offdie(dbg,offsetOfCu+offsetOfType,dieOfType,&err);
    if(DW_DLV_ERROR==res)
    {
      dwarfErrorHandler(err,NULL);
    }
    if(DW_DLV_NO_ENTRY==res)
    {
      fprintf(stderr,"no die at offset %i\n",(int)(offsetOfCu+offsetOfType));
      fflush(stderr);
      abort();
    }
  }
  
  char* name=getNameForDie(dbg,*dieOfType,cu);
  if(shouldFreeDieOfType)
  {
    free(dieOfType);
  }
  return name;
  
}

//gets the TypeInfo corresponding to the type
//referred to by the DW_AT_type attribute of the die in question
//returns NULL if the type doesn't exist
//or if the die doesn't have DW_AT_type
TypeInfo* getTypeInfoFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  Dwarf_Die dieOfType;
  char* name=getTypeNameFromATType(dbg,die,cu,&dieOfType);
  logprintf(ELL_INFO_V4,ELS_MISC,"name is %s\n",name);
  //todo: scoping? vars other than global?
  TypeInfo* result=dictGet(cu->tv->types,name);
  if(!result)
  {
    //it's possible we haven't read this die yet
    walkDieTree(dbg,dieOfType,cu,false);
  }
  result=dictGet(cu->tv->types,name);
  if(!result)
  {
    fprintf(stderr,"Type for die of name %s does not seem to exist\n",name);
    fflush(stderr);
  }
  return result;
}

void addBaseTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_BASE;
  type->cu=cu;//base types should be the same across all cu's in the given langauge, but we don't know that all cu's are the same language
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
  dictInsert(cu->tv->types,type->name,type);
  logprintf(ELL_INFO_V4,ELS_MISC,"added base type of name %s\n",type->name);
}

//read in the type definition of a structure
void addStructureFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"reading structure ");
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_STRUCT;
  type->cu=cu;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  logprintf(ELL_INFO_V4,ELS_MISC,"of name %s\n",type->name);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return;
  }
  type->length=byteSize;
  //insert the type into global types now with a note that
  //it's incomplete in case has members that reference it
  type->incomplete=true;
  dictInsert(cu->tv->types,type->name,type);
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
      type->fields=realloc(type->fields,type->numFields*sizeof(char*));
      MALLOC_CHECK(type->fields);
      type->fieldLengths=realloc(type->fieldLengths,type->numFields*sizeof(int));
      MALLOC_CHECK(type->fieldLengths);
      type->fieldTypes=realloc(type->fieldTypes,type->numFields*sizeof(char*));
      MALLOC_CHECK(type->fieldTypes);

      type->fields[idx]=getNameForDie(dbg,child,cu);
      logprintf(ELL_INFO_V4,ELS_MISC,"found field %s\n",type->fields[idx]);

      TypeInfo* typeOfField=getTypeInfoFromATType(dbg,child,cu);
      if(!typeOfField)
      {
        fprintf(stderr,"error, field type doesn't seem to exist, cannot create type %s\n",type->name);
        type->fieldTypes[idx]=NULL;
        freeTypeInfo(type);
        return;
      }
      
      //todo: perhaps use DW_AT_location instead of this?
      type->fieldLengths[idx]=typeOfField->length;
      type->fieldTypes[idx]=typeOfField;
      idx++;
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }
  else
  {
    fprintf(stderr,"structure %s with no members, this is odd\n",type->name);
    freeTypeInfo(type);//todo: is this really what we want to do
    return;
  }
  //check for fde info for transformation
  Dwarf_Attribute attr;
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  type->incomplete=false;
  logprintf(ELL_INFO_V4,ELS_MISC,"added structure type %s\n",type->name);
}

void addPointerTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_POINTER;
  type->cu=cu;
  Dwarf_Error err=0;
  logprintf(ELL_INFO_V4,ELS_MISC,"getting name for pointer\n");
  type->name=getNameForDie(dbg,die,cu);
  logprintf(ELL_INFO_V4,ELS_MISC,"got name for pointer\n");
  type->incomplete=true;//set incomplete and add in case the structure we get refers to it
  dictInsert(cu->tv->types,type->name,type);
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
    fprintf(stderr,"WARNING: adding pointer type %s when the type it points to does not exist\n",type->name);
    type->pointedType=NULL;
  }
  else
  {
    type->pointedType=pointedType;
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"added pointer type of name %s\n",type->name);
  //check for fde info for transformation
  Dwarf_Attribute attr;
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  type->incomplete=false;
}



void addArrayTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"reading array type\n");
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_ARRAY;
  type->cu=cu;
  Dwarf_Error err=0;
  type->name=getNameForDie(dbg,die,cu);
  TypeInfo* pointedType=getTypeInfoFromATType(dbg,die,cu);
  if(!pointedType)
  {
    death("ERROR: cannot add array with no type\n");
  }
  type->pointedType=pointedType;
  type->lowerBound=0;
  type->upperBound=0;
  Dwarf_Die child;
  int res=dwarf_child(die,&child,&err);
  if(res==DW_DLV_OK)
  {
    do
    {
      Dwarf_Half tag = 0;
      dwarf_tag(child,&tag,&err);
      if(tag!=DW_TAG_subrange_type)
      {
        fprintf(stderr,"within array type found tag not a subrange, this is bizarre\n");
        continue;
      }
      Dwarf_Attribute attr;
      
      int res=dwarf_attr(child,DW_AT_lower_bound,&attr,&err);
      if(res==DW_DLV_OK)
      {
        type->lowerBound=readAttributeAsInt(attr);
      }
      res=dwarf_attr(child,DW_AT_upper_bound,&attr,&err);
      if(res==DW_DLV_OK)
      {
        type->upperBound=readAttributeAsInt(attr);
      }
      
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }

  //check for fde info for transformation
  Dwarf_Attribute attr;
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  dictInsert(cu->tv->types,type->name,type);
  logprintf(ELL_INFO_V4,ELS_MISC,"added array type of name %s\n",type->name);
}

void addTypedefFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  char* name=getNameForDie(dbg,die,cu);
  TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
  if(type)
  {
    dictInsert(cu->tv->types,name,type);
    logprintf(ELL_INFO_V4,ELS_MISC,"added typedef for name %s\n",name);
  }
  else
  {
    fprintf(stderr,"WARNING: Unable to resolve typedef for %s\n",name);
  }
  free(name);
}

void addVarFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  VarInfo* var=zmalloc(sizeof(VarInfo));
  var->name=getNameForDie(dbg,die,cu);
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_declaration,&attr,&err);
  if(res==DW_DLV_OK)
  {
    int declaration=readAttributeAsInt(attr);
    if(declaration)
    {
      var->declaration=true;
    }
  }
  
  var->type=getTypeInfoFromATType(dbg,die,cu);
  if(!var->type)
  {
    fprintf(stderr,"Cannot create var %s when its type cannot be determined\n",var->name);
    free(var->name);
    return;
  }
  VarInfo* prev=dictGet(cu->tv->globalVars,var->name);
  if(prev)
  {
    if(prev->declaration)
    {
      dictSet(cu->tv->globalVars,var->name,var,&freeVarInfoVoid);
    }
    else
    {
      fprintf(stderr,"Multiple definitions of the global variable '%s', linker should have caught this",var->name);
      death(NULL);
    }
  }
  else
  {
    dictInsert(cu->tv->globalVars,var->name,var);
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"added variable %s\n",var->name);
}

void parseCompileUnit(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit** cu)
{
  *cu=zmalloc(sizeof(CompilationUnit));
  (*cu)->subprograms=dictCreate(100);//todo: get rid of magic # 100 and base it on something
  List* cuLi=zmalloc(sizeof(List));
  cuLi->value=*cu;
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
  (*cu)->tv=tv;
  tv->types=dictCreate(100);//todo: get rid of magic number 100 and base it on smth
  Dictionary* globalVars=dictCreate(100);//todo: get rid of magic number 100 and base it on smth
  assert(globalVars);
  tv->globalVars=globalVars;
  tv->parsedDies=integerMapCreate(100);//todo: get rid of magic number 100 and base it on smth
  //create the void type
  TypeInfo* voidType=zmalloc(sizeof(TypeInfo));
  voidType->type=TT_VOID;
  voidType->name=strdup("void");
  dictInsert(tv->types,voidType->name,voidType);
    
  (*cu)->name=getNameForDie(dbg,die,*cu);
  setIdentifierForCU(*cu,die);
  logprintf(ELL_INFO_V4,ELS_MISC,"compilation unit has name %s\n",(*cu)->name);
}

void addSubprogramFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  SubprogramInfo* prog=zmalloc(sizeof(SubprogramInfo));
  prog->name=getNameForDie(dbg,die,cu);
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_low_pc,&attr,&err);
  if(DW_DLV_OK==res)
  {
    prog->lowpc=readAttributeAsInt(attr);
  }
  res=dwarf_attr(die,DW_AT_high_pc,&attr,&err);
  if(DW_DLV_OK==res)
  {
    prog->highpc=readAttributeAsInt(attr);
  }
  dictInsert(cu->subprograms,prog->name,prog);
}

//takes a ** to a compile unit because if we parse a compile unit,
//the current compile unit will change
void parseDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit** cu,bool* parseChildren)
{
  Dwarf_Off off,cuOff;
  Dwarf_Error err;
  dwarf_dieoffset(die,&off,&err);
  dwarf_die_CU_offset(die,&cuOff,&err);
  logprintf(ELL_INFO_V4,ELS_MISC,"processing die at offset %i (%i)\n",(int)off,(int)cuOff);
  if(*cu && mapExists((*cu)->tv->parsedDies,&off))
  {
    //we've already parsed this die
    *parseChildren=false;//already will have parsed children too
    return;
  }

  Dwarf_Half tag = 0;
  dwarf_tag(die,&tag,&err);
  if(!(*cu) && tag!=DW_TAG_compile_unit)
  {
    death("tag before compile unit\n");
  }

  int* key=zmalloc(sizeof(int));
  *key=off;
  if(*cu)
  {
    //insert it into our set of parsed dies so that
    //it's impossible to enter an infinite loop with
    //a die referencing a die currently being processed
    //map requires memory allocated for the key
    mapInsert((*cu)->tv->parsedDies,key,"incomplete");
    *parseChildren=true;
  }
  

  
  switch(tag)
  {
  case DW_TAG_compile_unit:
    parseCompileUnit(dbg,die,cu);
    break;
  case DW_TAG_base_type:
    addBaseTypeFromDie(dbg,die,*cu);
    break;
  case DW_TAG_pointer_type:
    addPointerTypeFromDie(dbg,die,*cu);
    break;
  case DW_TAG_array_type:
    addArrayTypeFromDie(dbg,die,*cu);
    *parseChildren=false;
    break;
  case DW_TAG_structure_type:
    addStructureFromDie(dbg,die,*cu);
    *parseChildren=false;//reading the structure will have taken care of that
    break;
  case DW_TAG_typedef:
  case DW_TAG_const_type://const only changes program semantics, not memory layout, so we don't care about it really, just treat it as a typedef
    addTypedefFromDie(dbg,die,*cu);
    break;
  case DW_TAG_variable:
    addVarFromDie(dbg,die,*cu);
    break;
  case DW_TAG_subprogram:
    addSubprogramFromDie(dbg,die,*cu);
    break;
  case DW_TAG_formal_parameter: //ignore because if a function change will be recompiled and won't modify function while executing it
    break;

  default:
    fprintf(stderr,"Unknown die type 0x%x. Ignoring it but this may not be what you want\n",tag);
    
  }
  
  //insert the key as its value too because all we
  //care about is its existance, not its value
  mapSet((*cu)->tv->parsedDies,key,key,NULL,NULL);
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
  parseDie(dbg,die,&cu,&parseChildren);
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
DwarfInfo* readDWARFTypes(ElfInfo* elf)
{
  di=zmalloc(sizeof(DwarfInfo));
  Dwarf_Error err;
  Dwarf_Debug dbg;
  if(DW_DLV_OK!=dwarf_elf_init(elf->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  //code inspired by David Anderson's simplereader.c
  //distributed with libdwarf
  Dwarf_Unsigned nextCUHeader=0;//todo: does this need to be initialized outside
                                //the loop, doees dwarf_next_cu_header read it,
                                //or does it keep its own state and only set it?
  Dwarf_Unsigned cuHeaderLength=0;
  Dwarf_Half version=0;
  Dwarf_Unsigned abbrevOffset=0;
  Dwarf_Half addressSize=0;
  cuIdentifiers=dictCreate(100);//todo: get rid of magic number 100 and base it on smth
  while(1)
  {
    logprintf(ELL_INFO_V4,ELS_MISC,"iterating compilation units\n");
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
    CompilationUnit* cu=NULL;
    
    Dwarf_Die cu_die = 0;
    // The CU will have a single sibling, a cu_die.
    //passing NULL gets the first die in the CU
    if(DW_DLV_ERROR==dwarf_siblingof(dbg,NULL,&cu_die,&err))
    {
      dwarfErrorHandler(err,NULL);
    }
    if(res == DW_DLV_NO_ENTRY)
    {
      death("no entry! in dwarf_siblingof on CU die. This should never happen. Something is terribly wrong \n");
    }
    //walk the die tree without siblings because
    //we must initialize each compilation unit separately
    walkDieTree(dbg,cu_die,cu,true);
    dwarf_dealloc(dbg,cu_die,DW_DLA_DIE);
  }
  
  if(DW_DLV_OK!=dwarf_finish(dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  dictDelete(cuIdentifiers,NULL);
  elf->dwarfInfo=di;
  return di;
}
