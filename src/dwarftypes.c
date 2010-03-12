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
#include "util/refcounted.h"
#include "util/path.h"

Dictionary* cuIdentifiers=NULL;
DwarfInfo* di;
DList* activeSubprogramsHead=NULL;
DList* activeSubprogramsTail=NULL;
char* workingDir=NULL;

TypeInfo* getTypeInfoFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu);
char* getTypeNameFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,Dwarf_Die* dieOfType);

void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool siblings,ElfInfo* elf);


void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg)
{
  if(arg)
  {
    fprintf(stderr,"%s\n",(char*)arg);
  }
  death("Dwarf error: %s\n",dwarf_errmsg(err));
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
  case DW_FORM_strp:
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
      int res=dwarf_attr(die,DW_AT_comp_dir,&attr,&err);
      if(res==DW_DLV_OK)
      {
        char* dir=readAttributeAsString(attr);
        cu->id=malloc(strlen(cu->name)+strlen(dir)+3);
        sprintf(cu->id,"%s:%s",cu->name,dir);
        free(dir);
      }
      else
      {
        death("No way to find unique identifier for CU\n");
      }
  }
  else
  {
    cu->id=strdup(cu->name);
  }
  //don't actually care what value we insert, just making
  //a record that there's something
  dictInsert(cuIdentifiers,cu->id,cu->id);
}

void getRangeFromDie(Dwarf_Debug dbg,Dwarf_Die die,int* lowerBound,int* upperBound)
{
  Dwarf_Error err;
  *lowerBound=0;
  *upperBound=0;
  Dwarf_Die child;
  int res=dwarf_child(die,&child,&err);
  if(res==DW_DLV_OK)
  {
    do
    {
      //todo: support bounds of different types, dwarf allows the type
      //of the subrange to be specified
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
        *lowerBound=readAttributeAsInt(attr);
      }
      res=dwarf_attr(child,DW_AT_upper_bound,&attr,&err);
      if(res==DW_DLV_OK)
      {
        *upperBound=readAttributeAsInt(attr);
      }
      
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }

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
      snprintf(buf,64,"banon_%i",(int)offset);
    case DW_TAG_member:
      snprintf(buf,64,"manon_%i",(int)offset);
    case DW_TAG_variable:
      snprintf(buf,64,"vanon_%i",(int)offset);
      break;
    case DW_TAG_structure_type:
      snprintf(buf,64,"anon_struct_%u",(uint)offset);
      break;
    case DW_TAG_union_type:
      snprintf(buf,64,"anon_union_%u",(uint)offset);
      break;
    case DW_TAG_enumeration_type:
      snprintf(buf,64,"anon_enum_%u",(uint)offset);
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
    case DW_TAG_volatile_type:
      {
        char* name=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!name)
        {
          fprintf(stderr,"the type that DW_TAG_volatile_type references does not exist\n");
          fflush(stderr);
          abort();
        }
        snprintf(buf,2048,"volatile %s",name);
      }
      break;
    case DW_TAG_pointer_type:
      {
        char* namePointed=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!namePointed)
        {
          fprintf(stderr,"WARNING: the type that DW_TAG_pointer_type references does not exist\n");
          snprintf(buf,2048,"pgeneric%i*",(int)offset);
        }
        else
        {
          snprintf(buf,2048,"%s*",namePointed);
          free(namePointed);
        }
        
      }
      break;
    case DW_TAG_array_type:
      {
        char* namePointed=getTypeNameFromATType(dbg,die,cu,NULL);
        if(!namePointed)
        {
          death("the type that DW_TAG_array_type references does not exist\n");
        }
        int lowerBound=0,upperBound=0;
        getRangeFromDie(dbg,die,&lowerBound,&upperBound);
        snprintf(buf,2048,"%s[]_%i_%i",namePointed,lowerBound,upperBound);
        free(namePointed);
      }
      break;
    case DW_TAG_subprogram:
      snprintf(buf,64,"lambda_%i",(int)offset);
      break;
    case DW_TAG_lexical_block:
      snprintf(buf,64,"lex_blk_%i",(int)offset);
      break;
    case DW_TAG_formal_parameter:
      snprintf(buf,64,"formal_param_%i",(int)offset);
      break;
    case DW_TAG_subroutine_type:
      //I don't actually know what this is
      snprintf(buf,64,"sub_type_%i",(int)offset);
      break;
    default:
      snprintf(buf,64,"unknown_type_anon_%u",(uint)offset);
    }
    retname=strdup(buf);
  }
  else if(DW_DLV_OK==res)
  {
    retname=strdup(name);
    dwarf_dealloc(dbg,name,DW_DLA_STRING);
  }
  else
  {
    dwarfErrorHandler(err,NULL);
  }
  return retname;
}

Dwarf_Die getDieFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  Dwarf_Die result;
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_type,&attr,&err);
  if(res==DW_DLV_NO_ENTRY)
  {
    logprintf(ELL_INFO_V4,ELS_DWARFTYPES ,"no type entry\n");
    //this happens in gcc-generated stuff, there's no DWARF entry
    //for the void type, so we have to create it manually
    return NULL;
  }
  Dwarf_Half form;
  Dwarf_Off offsetOfType;
  dwarf_whatform(attr,&form,&err);
  //the returned form should be either local or global reference to a type
  if(form==DW_FORM_ref_addr)
  {
    dwarf_global_formref(attr,&offsetOfType,&err);
    //logprintf(ELL_INFO_V4,ELS_MISC,"global offset is %i\n",(int)offsetOfType);
    dwarf_offdie(dbg,offsetOfType,&result,&err);
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
    int res=dwarf_offdie(dbg,offsetOfCu+offsetOfType,&result,&err);
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
  return result;
}

//the main difference between this function
//and getTypeInfoFromATType aside from the return type
//is that this one respects typedefs, at least partially
//optional dieOfType argument is set to the die of the discovered type
//assuming it's found (i.e. will not be set if there was no DW_AT_type attribute)
//the returned pointer should be freed
char* getTypeNameFromATType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,Dwarf_Die* dieOfType)
{
  bool shouldFreeDieOfType=false;
  if(!dieOfType)
  {
    dieOfType=zmalloc(sizeof(Dwarf_Die));
    shouldFreeDieOfType=true;
  }
  *dieOfType=getDieFromATType(dbg,die,cu);
  char* name;
  if(*dieOfType)
  {
    name=getNameForDie(dbg,*dieOfType,cu);
  }
  else
  {
    //because Dwarf doesn't explicitly represent the void type
    name=strdup("void");
  }
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
  Dwarf_Die dieOfType=getDieFromATType(dbg,die,cu);
  void* data=NULL;
  if(dieOfType)
  {
    Dwarf_Error err;
    Dwarf_Off off;
    dwarf_dieoffset(dieOfType,&off,&err);
    data=mapGet(cu->tv->parsedDies,&off);
    if(!data)
    {
      //we haven't read in this die yet
      walkDieTree(dbg,dieOfType,cu,false,cu->elf);
      data=mapGet(cu->tv->parsedDies,&off);
    }
  }
  else
  {
    //void type
    data=dictGet(cu->tv->types,"void");
  }
  if(!data)
  {
    char* name=getNameForDie(dbg,dieOfType,cu);
    logprintf(ELL_WARN,ELS_DWARFTYPES,"Type for die of name %s does not seem to exist\n",name);
  }
  return data;
}

void setParsedDie(Dwarf_Die die,void* data,CompilationUnit* cu)
{
  Dwarf_Error err;
  Dwarf_Off off;
  dwarf_dieoffset(die,&off,&err);
  size_t* key=zmalloc(sizeof(size_t));
  *key=off;
  //set it properly in parsed dies so it can be referred to
  mapSet(cu->tv->parsedDies,key,data,NULL,free);
}



void* addBaseTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
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
    logprintf(ELL_WARN,ELS_DWARFTYPES,"base type %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return NULL;
  }
  type->length=byteSize;
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);
  logprintf(ELL_INFO_V4,ELS_MISC,"added base type of name %s\n",type->name);
  return type;
}

void* addEnumFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_ENUM;
  type->cu=cu;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    logprintf(ELL_WARN,ELS_DWARFTYPES,"enumeration %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return NULL;
  }
  type->length=byteSize;
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);
  //check for fde info for transformation
  Dwarf_Attribute attr;
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"added enum of name %s\n",type->name);
  return type;
}

//read in the type definition of a structure
void* addStructureFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"reading structure ");
  Dwarf_Error err=0;

  char* name=getNameForDie(dbg,die,cu);
  logprintf(ELL_INFO_V4,ELS_MISC,"of name %s\n",name);

  TypeInfo* type=dictGet(cu->tv->types,name);
  if(type)
  {
    if(!type->declaration)
    {
      death("Type %s already exists\n",name);
    }
  }
  else
  {
    type=zmalloc(sizeof(TypeInfo));
  }
  type->type=TT_STRUCT;
  type->cu=cu;
  type->name=name;
  
  Dwarf_Attribute attr;
  int res=dwarf_attr(die,DW_AT_declaration,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->declaration=(bool)readAttributeAsInt(attr);
  }

  
  //insert the type into global types now with a note that
  //it's incomplete in case has members that reference it
  dictSet(cu->tv->types,type->name,type,NULL);
  grabRefCounted((RC*)type);
  
  setParsedDie(die,type,cu);

  if(type->declaration)
  {
    //nothing more to read, it's just a declaration
    return type;
  }

  type->incomplete=true;
  

  Dwarf_Unsigned byteSize;
  res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return NULL;
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
        logprintf(ELL_ERR,ELS_DWARFTYPES,"field type doesn't seem to exist, cannot create type %s\n",type->name);
        death(NULL);//logprintf(ELL_ERR calls death, but just in case. . .
      }
      
      //todo: perhaps use DW_AT_location instead of this?
      type->fieldLengths[idx]=typeOfField->length;
      type->fieldTypes[idx]=typeOfField;
      grabRefCounted((RC*)typeOfField);
      idx++;
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }
  else
  {
    logprintf(ELL_WARN,ELS_DWARFTYPES,"structure %s with no members, this is odd\n",type->name);
  }
  //check for fde info for transformation
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  type->incomplete=false;
  logprintf(ELL_INFO_V4,ELS_MISC,"added structure type %s\n",type->name);
  return type;
}

void* addPointerTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  char* name=getNameForDie(dbg,die,cu);
  if(dictExists(cu->tv->types,name))
  {
    //this is an ok condition, because of typedefs a pointer
    //type could get added more than once
    return dictGet(cu->tv->types,name);
  }
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_POINTER;
  type->cu=cu;
  Dwarf_Error err=0;
  logprintf(ELL_INFO_V4,ELS_MISC,"getting name for pointer\n");
  type->name=name;
  logprintf(ELL_INFO_V4,ELS_MISC,"got name for pointer\n");
  type->incomplete=true;//set incomplete and add in case the structure we get refers to it
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);

  Dwarf_Off off;
  dwarf_dieoffset(die,&off,&err);
  size_t* key=zmalloc(sizeof(size_t));
  *key=off;
  //set it properly in parsed dies so it can be referred to
  mapSet(cu->tv->parsedDies,key,type,NULL,free);
  
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    fprintf(stderr,"structure %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return NULL;
  }
  type->length=byteSize;
  TypeInfo* pointedType=getTypeInfoFromATType(dbg,die,cu);
  if(!pointedType)
  {
    logprintf(ELL_WARN,ELS_DWARFTYPES,"adding pointer type %s when the type it points to does not exist\n",type->name);
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
  return type;
}



void* addArrayTypeFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"reading array type\n");
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_ARRAY;
  type->name=getNameForDie(dbg,die,cu);
  type->cu=cu;
  Dwarf_Error err=0;

  setParsedDie(die,type,cu);
  
  TypeInfo* pointedType=getTypeInfoFromATType(dbg,die,cu);
  if(!pointedType)
  {
    death("ERROR: cannot add array with no type\n");
  }
  type->pointedType=pointedType;
  getRangeFromDie(dbg,die,&type->lowerBound,&type->upperBound);


  //check for fde info for transformation
  Dwarf_Attribute attr;
  int res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);
  logprintf(ELL_INFO_V4,ELS_MISC,"added array type of name %s\n",type->name);
  return type;
}

void* addTypedefFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  char* name=getNameForDie(dbg,die,cu);
  TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
  if(type)
  {
    if(!dictExists(cu->tv->types,name))
    {
      dictInsert(cu->tv->types,name,type);
      grabRefCounted((RC*)type);
      logprintf(ELL_INFO_V4,ELS_MISC,"added typedef for name %s\n",name);
    }
  }
  else
  {
    fprintf(stderr,"WARNING: Unable to resolve typedef for %s\n",name);
  }
  free(name);
  return type;
}


//read in the type definition of a union
void* addUnionFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"reading union ");
  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_UNION;
  type->cu=cu;
  Dwarf_Error err=0;

  type->name=getNameForDie(dbg,die,cu);
  logprintf(ELL_INFO_V4,ELS_MISC,"of name %s\n",type->name);
  Dwarf_Unsigned byteSize;
  int res=dwarf_bytesize(die,&byteSize,&err);
  if(DW_DLV_NO_ENTRY==res)
  {
    logprintf(ELL_ERR,ELS_DWARFTYPES,"union %s has no byte length, we can't read it\n",type->name);
    freeTypeInfo(type);
    return NULL;
  }
  type->length=byteSize;
  //insert the type into global types now with a note that
  //it's incomplete in case has members that reference it
  type->incomplete=true;
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);
  setParsedDie(die,type,cu);
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
        logprintf(ELL_WARN,ELS_DWARFTYPES,"within union found tag not a member, this is bizarre\n");
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
        logprintf(ELL_ERR,ELS_DWARFTYPES,"field type doesn't seem to exist, cannot create type %s\n",type->name);
        death(NULL);//logprintf(ELL_ERR calls death, but just in case. . .
      }
      
      //todo: perhaps use DW_AT_location instead of this?
      type->fieldLengths[idx]=typeOfField->length;
      type->fieldTypes[idx]=typeOfField;
      grabRefCounted((RC*)typeOfField);
      idx++;
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }
  else
  {
    logprintf(ELL_WARN,ELS_DWARFTYPES,"union %s with no members, this is odd\n",type->name);
  }
  //check for fde info for transformation
  Dwarf_Attribute attr;
  res=dwarf_attr(die,DW_AT_MIPS_fde,&attr,&err);
  if(DW_DLV_OK==res)
  {
    type->fde=readAttributeAsInt(attr);
  }
  type->incomplete=false;
  logprintf(ELL_INFO_V4,ELS_MISC,"added union type %s\n",type->name);
  return type;
}


//parse formal parameters only to add their type to the
//list of types used by functions
void parseFormalParameter(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  TypeInfo* type=getTypeInfoFromATType(dbg,die,cu);
  if(!type)
  {
    logprintf(ELL_WARN,ELS_DWARFTYPES,"Found formal parameter with no type\n");
    return;
  }
  
  //we want to see if this parameter is inside any subprograms
  //(C does not allow nested subprograms, but some other language might
  //and it's just as easy to allow them here)
  //if the variable is inside a subprogram, we add its type to the list of types
  //inside that subprogram. This can be compared later against the set of types
  //that are to be patched later to determine whether the subprogram can have
  //an activation frame on the stack during patching
  DList* li=activeSubprogramsHead;
  for(;li;li=li->next)
  {
    SubprogramInfo* sub=li->value;
    assert(sub);
    List* li=zmalloc(sizeof(List));
    li->value=type;
    listAppend(&sub->typesHead,&sub->typesTail,li);
  }
}

void* addVarFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
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
    return NULL;
  }

  //variables are not global if we're reading inside a subprogram (function)
  if(!activeSubprogramsHead)
  {
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

  //we want to see if this variable is inside any subprograms
  //(C does not allow nested subprograms, but some other language might
  //and it's just as easy to allow them here)
  //if the variable is inside a subprogram, we add its type to the list of types
  //inside that subprogram. This can be compared later against the set of types
  //that are to be patched later to determine whether the subprogram can have
  //an activation frame on the stack during patching
  DList* li=activeSubprogramsHead;
  for(;li;li=li->next)
  {
    SubprogramInfo* sub=li->value;
    assert(sub);
    List* li=zmalloc(sizeof(List));
    li->value=var->type;
    listAppend(&sub->typesHead,&sub->typesTail,li);
  }
  return var;
}

void* parseCompileUnit(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit** cu,ElfInfo* elf)
{
  *cu=zmalloc(sizeof(CompilationUnit));
  (*cu)->elf=elf;
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
  tv->parsedDies=size_tMapCreate(100);//todo: get rid of magic number 100 and base it on smth
  //create the void type
  TypeInfo* voidType=zmalloc(sizeof(TypeInfo));
  voidType->type=TT_VOID;
  voidType->name=strdup("void");
  dictInsert(tv->types,voidType->name,voidType);
  grabRefCounted((RC*)voidType);  
  char* name=getNameForDie(dbg,die,*cu);
  char* dir=getDirectoryOfPath(elf->fname);
  char* relDir=makePathRelativeTo(dir,workingDir);
  free(dir);
  (*cu)->name=joinPaths(relDir,name);
  setIdentifierForCU(*cu,die);
  logprintf(ELL_INFO_V4,ELS_MISC,"compilation unit has name %s\n",(*cu)->name);
  return *cu;
}

SubprogramInfo* addSubprogramFromDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  SubprogramInfo* prog=zmalloc(sizeof(SubprogramInfo));
  prog->cu=cu;
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
  return prog;
}

//subroutineType supports function pointers
void* addSubroutineType(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu)
{
  Dwarf_Attribute attr;
  Dwarf_Error err;
  int res=dwarf_attr(die,DW_AT_prototyped,&attr,&err);
  int isPrototype=0;
  if(DW_DLV_OK==res)
  {
    isPrototype=readAttributeAsInt(attr);
  }
  if(!isPrototype)
  {
    death("non-prototype subroutine found, James doesn't yet know what to do with these\n");
  }

  TypeInfo* type=zmalloc(sizeof(TypeInfo));
  type->type=TT_SUBROUTINE_TYPE;
  type->cu=cu;

  type->name=getNameForDie(dbg,die,cu);
  logprintf(ELL_INFO_V4,ELS_DWARFTYPES,"reading subroutine type of name %s\n",type->name);
  type->length=0;//subroutine type in a way isn't a real type. It certainly has no length
  
  //insert the type into global types now with a note that
  //it's incomplete in case has members that reference it
  type->incomplete=true;
  dictInsert(cu->tv->types,type->name,type);
  grabRefCounted((RC*)type);
  setParsedDie(die,type,cu);
  Dwarf_Die child;
  res=dwarf_child(die,&child,&err);
  if(res==DW_DLV_OK)
  {
    int idx=0;
    do
    {
      //iterate through each formal parameter
      //todo: do we really need to do this? Not really using them for anything
      Dwarf_Half tag = 0;
      dwarf_tag(child,&tag,&err);
      if(tag!=DW_TAG_formal_parameter)
      {
        logprintf(ELL_WARN,ELS_DWARFTYPES,"within subroutine type found tag not a formal parameter, this is bizarre\n");
        continue;
      }
      type->numFields++;
      type->fields=realloc(type->fields,type->numFields*sizeof(char*));
      MALLOC_CHECK(type->fields);
      type->fieldTypes=realloc(type->fieldTypes,type->numFields*sizeof(char*));
      MALLOC_CHECK(type->fieldTypes);
      type->fields[idx]=getNameForDie(dbg,child,cu);

      TypeInfo* typeOfField=getTypeInfoFromATType(dbg,child,cu);
      if(!typeOfField)
      {
        logprintf(ELL_ERR,ELS_DWARFTYPES,"parameter type doesn't seem to exist, cannot create type %s\n",type->name);
        death(NULL);//logprintf(ELL_ERR calls death, but just in case. . .
      }
      
      //todo: perhaps use DW_AT_location instead of this?
      type->fieldTypes[idx]=typeOfField;
      grabRefCounted((RC*)typeOfField);
      grabRefCounted((RC*)typeOfField);
      idx++;
    }while(DW_DLV_OK==dwarf_siblingof(dbg,child,&child,&err));
  }

  type->incomplete=false;
  logprintf(ELL_INFO_V4,ELS_MISC,"added union type %s\n",type->name);
  return type;
}

//takes a ** to a compile unit because if we parse a compile unit,
//the current compile unit will change
//returns a void* datum which will be passed to endDieChildren when all children
//of the die have been parsed
void* parseDie(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit** cu,bool* parseChildren,ElfInfo* elf)
{
  void* result=NULL;
  Dwarf_Off off,cuOff;
  Dwarf_Error err;
  dwarf_dieoffset(die,&off,&err);
  dwarf_die_CU_offset(die,&cuOff,&err);
  logprintf(ELL_INFO_V4,ELS_MISC,"processing die at offset %i (%i)\n",(int)off,(int)cuOff);
  if(*cu && mapExists((*cu)->tv->parsedDies,&off))
  {
    //we've already parsed this die
    *parseChildren=false;//already will have parsed children too
    return result;
  }

  Dwarf_Half tag = 0;
  dwarf_tag(die,&tag,&err);
  if(!(*cu) && tag!=DW_TAG_compile_unit)
  {
    death("tag before compile unit\n");
  }

  size_t* key=zmalloc(sizeof(size_t));
  *key=off;
  if(*cu)
  {
    *parseChildren=true;
  }
  

  
  switch(tag)
  {
  case DW_TAG_compile_unit:
    result=parseCompileUnit(dbg,die,cu,elf);
    break;
  case DW_TAG_base_type:
    result=addBaseTypeFromDie(dbg,die,*cu);
    break;
  case DW_TAG_pointer_type:
    result=addPointerTypeFromDie(dbg,die,*cu);
    break;
  case DW_TAG_array_type:
    result=addArrayTypeFromDie(dbg,die,*cu);
    *parseChildren=false;
    break;
  case DW_TAG_structure_type:
    result=addStructureFromDie(dbg,die,*cu);
    *parseChildren=false;//reading the structure will have taken care of that
    break;
  case DW_TAG_union_type:
    result=addUnionFromDie(dbg,die,*cu);
    *parseChildren=false;//reading the union will have taken care of that
    break;
  case DW_TAG_enumeration_type:
    result=addEnumFromDie(dbg,die,*cu);
    *parseChildren=false;//enum's children will be the different
                         //enumeration values, which we don't actually
                         //care about
    break;
  case DW_TAG_typedef:
  case DW_TAG_volatile_type:
  case DW_TAG_const_type://const/volatile only changes program semantics, not memory layout, so we don't care about it really, just treat it as a typedef
    result=addTypedefFromDie(dbg,die,*cu);
    break;
  case DW_TAG_variable:
    result=addVarFromDie(dbg,die,*cu);
    break;
  case DW_TAG_subprogram:
    {
    //we actually do want to read inside the function because we want
    //to know what types it may be using, so we don't set parseChildren to false,
    //as a first impulse might be
    DList* li=zmalloc(sizeof(DList));
    li->value=addSubprogramFromDie(dbg,die,*cu);
    dlistAppend(&activeSubprogramsHead,&activeSubprogramsTail,li);
    result=li->value;
    }
    break;
  case DW_TAG_formal_parameter:
    parseFormalParameter(dbg,die,*cu);
    break;
    break;
  case DW_TAG_lexical_block:
    //we may care about its children, but not its definition itself
    break;
  case DW_TAG_subroutine_type:
    result=addSubroutineType(dbg,die,*cu);
    break;
  default:
    logprintf(ELL_WARN,ELS_DWARFTYPES,"Unknown die type 0x%x. Ignoring it but this may not be what you want\n",tag);
    
  }

  if(result || !mapExists((*cu)->tv->parsedDies,key))
  {
    mapSet((*cu)->tv->parsedDies,key,result,NULL,NULL);
  }
  return result;
}

void endDieChildren(Dwarf_Die die,void* data)
{
  Dwarf_Half tag = 0;
  Dwarf_Error err;
  dwarf_tag(die,&tag,&err);
  switch(tag)
  {
  case DW_TAG_subprogram:
    //assume that subprograms are properly nested, makes no sense
    //for a language to do otherwise
    assert(activeSubprogramsTail->value==data);
    dlistDeleteTail(&activeSubprogramsHead,&activeSubprogramsTail);
    break;
  }
}

void walkDieTree(Dwarf_Debug dbg,Dwarf_Die die,CompilationUnit* cu,bool siblings,ElfInfo* elf)
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
  void* data=parseDie(dbg,die,&cu,&parseChildren,elf);
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
      walkDieTree(dbg,childOrSibling,cu,true,elf);
      dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);
    }
  }
  endDieChildren(die,data);
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
  walkDieTree(dbg,childOrSibling,cu,true,elf);
  dwarf_dealloc(dbg,childOrSibling,DW_DLA_DIE);

}

//the returned structure should be freed
//when the caller is finished with it
//workingDir is used for path names
//it is the directory that names should be relative to
DwarfInfo* readDWARFTypes(ElfInfo* elf,char* workingDir_)
{
  workingDir=workingDir_;
  if(!elf->sectionIndices[ERS_DEBUG_INFO])
  {
    death("ELF file %s does not seem to have any dwarf DIE information\n",elf->fname);
  }
  
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
    walkDieTree(dbg,cu_die,cu,true,elf);
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
