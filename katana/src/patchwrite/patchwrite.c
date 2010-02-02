/*
  File: patchwrite.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Write patch information out to a po (patch object) file
*/

#include "types.h"
#include "dwarftypes.h"
#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <libdwarf.h>
#include "dwarf_instr.h"
#include <dwarf.h>
#include <assert.h>
#include "register.h"

ElfInfo* oldBinary=NULL;

typedef struct
{
  VarInfo* var;
  TypeTransform* transform;
} VarTransformation;

Elf *outelf;

//Elf_Data* patch_syms_rel_data=NULL;
//Elf_Data* patch_syms_new_data=NULL;
Elf_Data* patch_rules_data=NULL;
Elf_Data* patch_expr_data=NULL;
Elf_Data* strtab_data=NULL;
Elf_Data* symtab_data=NULL;
//Elf_Data* old_symtab_data=NULL;

//Elf_Scn* patch_syms_rel_scn=NULL;
//Elf_Scn* patch_syms_new_scn=NULL;

Elf_Scn* patch_rules_scn=NULL;
Elf_Scn* patch_expr_scn=NULL;
Elf_Scn* strtab_scn=NULL;
Elf_Scn* symtab_scn=NULL;
//now not writing old symbols b/c
//better to get the from /proc/PID/exe
/*Elf_Scn* old_symtab_scn=NULL;//relevant symbols from the symbol table of the old binary
                             //we must store this in the patch object in case the object
                             //in memory does not have a symbol table loaded in memory
                             */

Dwarf_P_Debug dbg;
Dwarf_P_Die rootDie=NULL;
Dwarf_P_Die lastDie=NULL;//keep track of for sibling purposes
Dwarf_Unsigned cie;//index of cie we're using

//adds an entry to the string table, return its offset
int addStrtabEntry(char* str)
{
  int len=strlen(str)+1;
  //we have co-opted the d_off field of Elf_Data to determine
  //where in the d_buf to start writing. It will be zeroed before
  //the elf file is actually written. We allocate larger memory
  //for d_buf as needed
  if(len>strtab_data->d_size-strtab_data->d_off)
  {
    //ran out of space, allocate more
    strtab_data->d_size=max(strtab_data->d_size*2,len*2);
    strtab_data->d_buf=realloc(strtab_data->d_buf,strtab_data->d_size);
  }
  memcpy((char*)strtab_data->d_buf+strtab_data->d_off,str,len);
  strtab_data->d_off+=len;
  return strtab_data->d_off-len;
}

//return index of entry in symbol table
int addSymtabEntry(Elf_Data* data,Elf32_Sym* sym)
{
  int len=sizeof(Elf32_Sym);
  //we have co-opted the d_off field of Elf_Data to determine
  //where in the d_buf to start writing. It will be zeroed before
  //the elf file is actually written. We allocate larger memory
  //for d_buf as needed
  if(len>data->d_size-data->d_off)
  {
    //ran out of space, allocate more
    data->d_size=max(data->d_size*2,len*2);
    data->d_buf=realloc(data->d_buf,data->d_size);
  }
  memcpy((char*)data->d_buf+data->d_off,sym,len);
  data->d_off+=len;
  return (data->d_off-len)/len;
}

void writeTypeToDwarf(TypeInfo* type)
{
  if(type->die)
  {
    return;//already written
  }
  int tag;
  switch(type->type)
  {
  case TT_STRUCT:
    tag=DW_TAG_structure_type;
    break;
  case TT_BASE:
    tag=DW_TAG_base_type;
    break;
  case TT_POINTER:
    tag=DW_TAG_pointer_type;
    break;
  case TT_ARRAY:
    tag=DW_TAG_array_type;
    break;
  case TT_VOID:
    return;//don't actually need to write out the void type, DWARF doesn't represent it
    break;
  default:
    death("unknown type type\n");
  }
  Dwarf_P_Die parent=lastDie?NULL:rootDie;
  Dwarf_Error err;
  Dwarf_P_Die die=dwarf_new_die(dbg,tag,parent,NULL,lastDie,NULL,&err);
  lastDie=die;
  type->die=die;
  dwarf_add_AT_name(die,type->name,&err);
  dwarf_add_AT_unsigned_const(dbg,die,DW_AT_byte_size,type->length,&err);
  if((TT_POINTER==type->type || TT_ARRAY==type->type) && TT_VOID!=type->pointedType->type)
  {
    writeTypeToDwarf(type->pointedType);
    dwarf_add_AT_reference(dbg,die,DW_AT_type,type->pointedType->die,&err);
  }
  if(TT_ARRAY==type->type)
  {
    dwarf_add_AT_signed_const(dbg,die,DW_AT_lower_bound,type->lowerBound,&err);
    dwarf_add_AT_signed_const(dbg,die,DW_AT_upper_bound,type->upperBound,&err);
  }
  else if(TT_STRUCT==type->type)
  {
    Dwarf_P_Die lastMemberDie=NULL;
    for(int i=0;i<type->numFields;i++)
    {
      writeTypeToDwarf(type->fieldTypes[i]);
      parent=0==i?die:NULL;
      Dwarf_P_Die memberDie=dwarf_new_die(dbg,DW_TAG_member,parent,NULL,lastMemberDie,NULL,&err);
      dwarf_add_AT_name(memberDie,type->fields[i],&err);
      lastMemberDie=memberDie;
      //todo: do we really need to add in all the other stuff
      //remember that we're really just writing in the types
      //to identify connections to the FDEs for fixing up types.
    }
  }
}

void writeVarToDwarf(VarInfo* var)
{
  Dwarf_Error err;
  writeTypeToDwarf(var->type);
  Dwarf_P_Die die=dwarf_new_die(dbg,DW_TAG_variable,NULL,NULL,lastDie,NULL,&err);
  dwarf_add_AT_name(die,var->name,&err);
  dwarf_add_AT_reference(dbg,die,DW_AT_type,var->type->die,&err);
  lastDie=die;
}

void writeTransformationToDwarf(TypeTransform* trans)
{
  if(trans->onDisk)
  {
    printf("transformation already on disk %i,%i\n",trans->onDisk,true);
    return;
  }
  trans->onDisk=true;
  Dwarf_Error err;
  Dwarf_P_Fde fde=dwarf_new_fde(dbg,&err);
  assert(trans->to->die);
  dwarf_add_frame_fde(dbg,fde,trans->to->die,cie,0,0,0,&err);
  DwarfInstructions instrs;
  memset(&instrs,0,sizeof(DwarfInstructions));
  int oldBytesSoFar=0;
  for(int i=0;i<trans->from->numFields;i++)
  {
    DwarfInstruction inst;
    int off=trans->fieldOffsets[i];
    if(FIELD_DELETED==off)
    {
      oldBytesSoFar+=trans->from->fieldLengths[i];
      continue;
    }
    else if(FIELD_RECURSE==off)
    {
      inst.opcode=DW_CFA_KATANA_do_fixups;
      inst.arg1=0;//todo: this is wrong. Really need to refer to another FDE, but we
                  //may have to convert to disk representation first before
                  //we can get that info
      //todo: probably will need to build up lists of these
      //and add them in at the end. Could potentially refer to DIE instead
      //and just get all dies set before dealing with FDEs at all
      addInstruction(&instrs,&inst);
      oldBytesSoFar+=trans->from->fieldLengths[i];
      continue;
    }
    printf("adding normal field to fde\n");
    //transforming a struct with fields that are base types, nice and easy
    inst.opcode=DW_CFA_register;
    int size=trans->from->fieldTypes[i]->length;
    byte bytes[6];
    bytes[0]=ERT_CURR_TARG_NEW;
    assert(size<256);
    bytes[1]=(byte)size;
    //todo: waste of space, whole point of LEB128, could put this in one
    //byte instead of 4 most of the time
    assert(sizeof(int)==4);
    memcpy(bytes+2,&off,4);
    inst.arg1Bytes=encodeAsLEB128(bytes,6,false,&inst.arg1NumBytes);
    bytes[0]=ERT_CURR_TARG_OLD;
    memcpy(bytes+2,&oldBytesSoFar,4);
    inst.arg2Bytes=encodeAsLEB128(bytes,6,false,&inst.arg2NumBytes);
    addInstruction(&instrs,&inst);
    free(inst.arg1Bytes);
    oldBytesSoFar+=trans->from->fieldLengths[i];

  }
  printf("adding %i bytes to fde\n",instrs.numBytes);
  dwarf_insert_fde_inst_bytes(dbg,fde,instrs.numBytes,instrs.instrs,&err);
}

//takes a list of var transformations and actually constructs the ELF sections
void writeVarTransforms(List* varTrans)
{
  List* li=varTrans;
  for(;li;li=li->next)
  {
    VarTransformation* vt=li->value;
    writeVarToDwarf(vt->var);
    writeTransformationToDwarf(vt->transform);
    printf("writing transformation for var %s\n",vt->var->name);
    /*
    //create the symbol entry
    //this means first creating the string table entry
    int strIdx=addStrtabEntry(vt->var->name);
    Elf32_Sym sym;
    sym.st_name=strIdx;
    sym.st_value=0;//don't know yet where this symbol is going to end up
    sym.st_size=vt->var->type->length;
    //todo: support local, weak, etc symbols
    sym.st_info=ELF32_ST_INFO(STB_GLOBAL,STT_OBJECT);
    sym.st_other=0;
    sym.st_shndx=0;//no special section this is related to;
    addSymtabEntry(patch_syms_rel_data,&sym);*/
  }
}

void writeTypeTransformationInfo(DwarfInfo* diPatchee,DwarfInfo* diPatched)
{
  List* varTransHead=NULL;
  List* varTransTail=NULL;
  
      //todo: handle addition of variables and also handle
  //      things moving between compilation units,
  //      perhaps group global objects from all compilation units
  //      together before dealing with them.
  List* cuLi1=diPatchee->compilationUnits;
  for(;cuLi1;cuLi1=cuLi1->next)
  {
    CompilationUnit* cu1=cuLi1->value;
    CompilationUnit* cu2=NULL;
    //find the corresponding compilation unit in the patched process
    //note: we assume that the number of compilation units is not so large
    //that using a hash table would give us much better performance than
    //just going through a list. If working on a truly enormous project it
    //might make sense to do this
    List* cuLi2=diPatched->compilationUnits;
    for(;cuLi2;cuLi2=cuLi2->next)
    {
      cu2=cuLi2->value;
      if(cu1->name && cu2->name && !strcmp(cu1->name,cu2->name))
      {
        break;
      }
      cu2=NULL;
    }
    if(!cu2)
    {
      fprintf(stderr,"WARNING: the patched version omits an entire compilation unit present in the original version.\n");
      if(cu1->name)
      {
        fprintf(stderr,"Missing cu is named %s\n",cu1->name);
      }
      else
      {
        fprintf(stderr,"The compilation unit in the patchee version does not have a name\n");
      }
      //todo: we also need to handle a compilation unit being present
      //in the patched version and not the patchee
      break;
    }
    cu1->presentInOtherVersion=true;
    cu2->presentInOtherVersion=true;
    
    TransformationInfo* trans=zmalloc(sizeof(TransformationInfo));
    trans->typeTransformers=dictCreate(100);//todo: get rid of magic # 100
    printf("Examining compilation unit %s\n",cu1->name);
    VarInfo** vars1=(VarInfo**)dictValues(cu1->tv->globalVars);
    //todo: handle addition of variables in the patch
    VarInfo* var=vars1[0];
    Dictionary* patchVars=cu2->tv->globalVars;
    for(int i=0;var;i++,var=vars1[i])
    {
      printf("Found variable %s \n",var->name);
      VarInfo* patchedVar=dictGet(patchVars,var->name);
      if(!patchedVar)
      {
        //todo: do we need to do anything special to handle removal of variables in the patch?
        printf("warning: var %s seems to have been removed in the patch\n",var->name);
        continue;
      }
      TypeInfo* ti1=var->type;
      TypeInfo* ti2=patchedVar->type;
      bool needsTransform=false;
      if(dictExists(trans->typeTransformers,ti1->name))
      {
        needsTransform=true;
      }
      else
      {
        //todo: should have some sort of caching for types we've already
        //determined to be equal
        TypeTransform* transform=NULL;
        if(!compareTypes(ti1,ti2,&transform))
        {
          if(!transform)
          {
            //todo: may not want to actually abort, may just want to issue
            //an error
            fprintf(stderr,"Error, cannot generate type transformation for variable %s\n",var->name);
            fflush(stderr);
            abort();
          }
          printf("generated type transformation for type %s\n",ti1->name);
          dictInsert(trans->typeTransformers,ti1->name,transform);
          needsTransform=true;
        }
      }
      if(needsTransform)
      {
        List* li=zmalloc(sizeof(List));
        VarTransformation* vt=zmalloc(sizeof(VarTransformation));
        vt->var=patchedVar;
        vt->transform=(TypeTransform*)dictGet(trans->typeTransformers,var->type->name);
        if(!vt->transform)
        {
          death("the transformation info for that variable doesn't exist!\n");
        }
        li->value=vt;
        if(varTransHead)
        {
          varTransTail->next=li;
        }
        else
        {
          varTransHead=li;
        }
        varTransTail=li;
      }
    }

    printf("completed all transformations for compilation unit %s\n",cu1->name);
    //freeTransformationInfo(trans);//this was causing memory errors. todo: debug it
    //I think it's needed for writeVarTransforms later. Should do reference counting
    free(vars1);
  }
  writeVarTransforms(varTransHead);
}

void createSections(Elf* outelf)
{
  //first create the string table
  strtab_scn=elf_newscn(outelf);
  strtab_data=elf_newdata(strtab_scn);
  strtab_data->d_align=1;
  strtab_data->d_buf=malloc(8);//arbitrary starting size, more will be allocced as needed
  strtab_data->d_off=0;
  strtab_data->d_size=8;//again, will increase as needed
  strtab_data->d_version=EV_CURRENT;
  
  Elf32_Shdr* shdr ;
  shdr=elf32_getshdr(strtab_scn);
  shdr->sh_type=SHT_STRTAB;
  shdr->sh_link=SHN_UNDEF;
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=1;//first real entry in the string table


  
  addStrtabEntry("");//first entry in stringtab null so can have normal unnamed null section
                     //todo: what is the purpose of this?
  addStrtabEntry(".strtab");

  /*
  //now create the patch syms to relocate
  patch_syms_rel_scn=elf_newscn(outelf);
  patch_syms_rel_data=elf_newdata(patch_syms_rel_scn);
  patch_syms_rel_data->d_align=1;
  patch_syms_rel_data->d_buf=malloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  patch_syms_rel_data->d_off=0;
  patch_syms_rel_data->d_size=8;//again, will increase as needed
  patch_syms_rel_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_syms_rel_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".patch_syms_rel");

  //now create the patch syms for new variables/functions
  patch_syms_new_scn=elf_newscn(outelf);
  patch_syms_new_data=elf_newdata(patch_syms_new_scn);
  patch_syms_new_data->d_align=1;
  patch_syms_new_data->d_buf=malloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  patch_syms_new_data->d_off=0;
  patch_syms_new_data->d_size=8;//again, will increase as needed
  patch_syms_new_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_syms_new_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".patch_syms_new");*/

  //now create patch rules
  patch_rules_scn=elf_newscn(outelf);
  patch_rules_data=elf_newdata(patch_rules_scn);
  patch_rules_data->d_align=1;
  patch_rules_data->d_buf=zmalloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  patch_rules_data->d_off=0;
  patch_rules_data->d_size=8;//again, will increase as needed
  patch_rules_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_rules_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_rules");

  //now create patch expressions
  patch_expr_scn=elf_newscn(outelf);
  patch_expr_data=elf_newdata(patch_expr_scn);
  patch_expr_data->d_align=1;
  patch_expr_data->d_buf=zmalloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  patch_expr_data->d_off=0;
  patch_expr_data->d_size=8;//again, will increase as needed
  patch_expr_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(patch_expr_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_expr");

  //ordinary symtab
  symtab_scn=elf_newscn(outelf);
  symtab_data=elf_newdata(symtab_scn);
  symtab_data->d_align=1;
  symtab_data->d_buf=malloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  symtab_data->d_off=0;
  symtab_data->d_size=8;//again, will increase as needed
  symtab_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(symtab_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".symtab");

  //now not using old symtab b/c better to get old symbols from
  // /proc/PID/exe
  /*
  //symtab for symbols we care about in old binary
  old_symtab_scn=elf_newscn(outelf);
  old_symtab_data=elf_newdata(old_symtab_scn);
  old_symtab_data->d_align=1;
  old_symtab_data->d_buf=malloc(8);//arbitrary starting size, more
                                       //will be allocced as needed
  old_symtab_data->d_off=0;
  old_symtab_data->d_size=8;//again, will increase as needed
  old_symtab_data->d_version=EV_CURRENT;
  
  shdr=elf32_getshdr(old_symtab_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=4;//32-bit
  shdr->sh_entsize=sizeof(Elf32_Sym);
  shdr->sh_name=addStrtabEntry(".symtab.v0");
  */
}

void finalizeDataSizes()
{
  //our functions for dealing with dynamically increasing the size of the data
  //sections coopted some of the fields for their own use. Now put them back
  //to what they should be

  //first strtab
  strtab_data->d_size=strtab_data->d_off;//what was actually used
  strtab_data->d_off=0;
  strtab_data->d_buf=realloc(strtab_data->d_buf,strtab_data->d_size);
  Elf32_Shdr* shdr=elf32_getshdr(strtab_scn);
  shdr->sh_size=strtab_data->d_size;

  /* //then patch syms rel
  patch_syms_rel_data->d_size=patch_syms_rel_data->d_off;//what was actually used
  patch_syms_rel_data->d_off=0;
  patch_syms_rel_data->d_buf=realloc(patch_syms_rel_data->d_buf,
                                     patch_syms_rel_data->d_size);
  shdr=elf32_getshdr(patch_syms_rel_scn);
  shdr->sh_size=patch_syms_rel_data->d_size;

  //then patch syms new
  patch_syms_new_data->d_size=patch_syms_new_data->d_off;//what was actually used
  patch_syms_new_data->d_off=0;
  patch_syms_new_data->d_buf=realloc(patch_syms_new_data->d_buf,
                                     patch_syms_new_data->d_size);
  shdr=elf32_getshdr(patch_syms_new_scn);
  shdr->sh_size=patch_syms_new_data->d_size;*/

  //then patch expressions
  patch_expr_data->d_size=patch_expr_data->d_off;//what was actually used
  patch_expr_data->d_off=0;
  patch_expr_data->d_buf=realloc(patch_expr_data->d_buf,patch_expr_data->d_size);
  shdr=elf32_getshdr(patch_expr_scn);
  shdr->sh_size=patch_expr_data->d_size;

  //ordinary symtab
  symtab_data->d_size=symtab_data->d_off;//what was actually used
  symtab_data->d_off=0;
  symtab_data->d_buf=realloc(symtab_data->d_buf,
                                     symtab_data->d_size);
  shdr=elf32_getshdr(symtab_scn);
  shdr->sh_size=symtab_data->d_size;

  //now not using old symtab b/c better to get old symbols from
  // /proc/PID/exe
  /*
  //symtab from old binary
  old_symtab_data->d_size=old_symtab_data->d_off;//what was actually used
  old_symtab_data->d_off=0;
  old_symtab_data->d_buf=realloc(old_symtab_data->d_buf,
                                     old_symtab_data->d_size);
  shdr=elf32_getshdr(old_symtab_scn);
  shdr->sh_size=old_symtab_data->d_size;
  */
}

int dwarfWriteSectionCallback(char* name,int size,Dwarf_Unsigned type,
                              Dwarf_Unsigned flags,Dwarf_Unsigned link,
                              Dwarf_Unsigned info,int* sectNameIdx,int* error)
{
  //todo: write this section
  Elf_Scn* scn=elf_newscn(outelf);
  Elf32_Shdr* shdr=elf32_getshdr(scn);
  shdr->sh_name=addStrtabEntry(name);
  shdr->sh_type=type;
  shdr->sh_flags=flags;
  shdr->sh_size=size;
  shdr->sh_link=link;
  shdr->sh_info=info;
  Elf32_Sym sym;
  sym.st_name=shdr->sh_name;
  sym.st_value=0;//don't yet know where this symbol will end up. todo: fix this, so relocations can theoretically be done
  sym.st_size=0;
  sym.st_info=ELF32_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_other=0;
  sym.st_shndx=elf_ndxscn(scn);
  *sectNameIdx=addSymtabEntry(symtab_data,&sym);
  printf("symbol index is %i\n",*sectNameIdx);
  *error=0;  
  return sym.st_shndx;
}

void writePatch(DwarfInfo* diPatchee,DwarfInfo* diPatched,char* fname,ElfInfo* oldBinary_)
{
  oldBinary=oldBinary_;
  int outfd = creat(fname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", fname);
  }
  outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
  Elf32_Ehdr* ehdr=elf32_newehdr(outelf);
  if(!ehdr)
  {
    death("Unable to create new ehdr\n");
  }
  ehdr->e_ident[EI_MAG0]=ELFMAG0;
  ehdr->e_ident[EI_MAG1]=ELFMAG1;
  ehdr->e_ident[EI_MAG2]=ELFMAG2;
  ehdr->e_ident[EI_MAG3]=ELFMAG3;
  ehdr->e_ident[EI_CLASS]=ELFCLASS32;
  ehdr->e_ident[EI_DATA]=ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION]=EV_CURRENT;
  ehdr->e_ident[EI_OSABI]=ELFOSABI_NONE;
  ehdr->e_machine=EM_386;
  ehdr->e_type=ET_NONE;//not relocatable, or executable, or shared object, or core, etc
  ehdr->e_version=EV_CURRENT;

  createSections(outelf);
  ehdr->e_shstrndx=elf_ndxscn(strtab_scn);//set strtab in elf header

  Dwarf_Error err;
  dbg=dwarf_producer_init(DW_DLC_WRITE,dwarfWriteSectionCallback,dwarfErrorHandler,NULL,&err);
  rootDie=dwarf_new_die(dbg,DW_TAG_compile_unit,NULL,NULL,NULL,NULL,&err);
  cie=dwarf_add_frame_cie(dbg,"",1,1,0,NULL,0,&err);
  if(DW_DLV_NOCOUNT==cie)
  {
    dwarfErrorHandler(err,"creating frame cie failed");
  }
  
  //now that we've created the necessary things, actually run through
  //the stuff to write in our data
  writeTypeTransformationInfo(diPatchee,diPatched);

  dwarf_add_die_to_debug(dbg,rootDie,&err);
  int numSections=dwarf_transform_to_disk_form(dbg,&err);
  printf("num dwarf sections is %i\n",numSections);
  for(int i=0;i<numSections;i++)
  {
    Dwarf_Signed elfScnIdx;
    Dwarf_Unsigned length;
    Dwarf_Ptr buf=dwarf_get_section_bytes(dbg,i,&elfScnIdx,&length,&err);
    Elf_Scn* scn=elf_getscn(outelf,elfScnIdx);
    Elf_Data* data=elf_newdata(scn);
    Elf32_Shdr* shdr=elf32_getshdr(scn);
    shdr->sh_size=length;
    data->d_size=length;
    data->d_off=0;
    data->d_buf=zmalloc(length);
    memcpy(data->d_buf,buf,length);
  }
  finalizeDataSizes();

  //now I don't remember what this was for and it seems to cause problems
  //elf_flagelf (outelf, ELF_C_SET, ELF_F_LAYOUT);


  if (elf_update (outelf, ELF_C_WRITE) <0)
  {
    fprintf(stderr,"Failed to write out elf file: %s\n",elf_errmsg (-1));
    exit(1);
  }
  

  elf_end (outelf);
  close (outfd);
  printf("wrote elf file %s\n",fname);
}

