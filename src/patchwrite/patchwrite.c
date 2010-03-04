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
#include "codediff.h"
#include "relocation.h"
#include "symbol.h"
#include "typediff.h"
#include "util/logging.h"
#include "elfwriter.h"
#include "sourcetree.h"

ElfInfo* oldBinary=NULL;
ElfInfo* newBinary=NULL;


ElfInfo* patch;
addr_t rodataOffset;//used to keep track of how much rodata we've used
Elf *outelf;


Dwarf_P_Debug dbg;
Dwarf_P_Die firstCUDie=NULL;
Dwarf_P_Die lastCUDie=NULL;//keep track of for sibling purposes
Dwarf_Unsigned cie;//index of cie we're using

void writeOldTypeToDwarf(TypeInfo* type,CompilationUnit* cuToWriteIn);

idx_t addSymbolFromBinaryToPatch(ElfInfo* binary,idx_t symIdx)
{
  assert(STN_UNDEF!=symIdx);
  //we expect this function to be called with binary equal to the
  //ELF object for one of the .o files. There may in fact, however,
  //be more information about the symbol in the new binary

  symIdx=reindexSymbol(binary,newBinary,symIdx,ESFF_FUZZY_MATCHING_OK);
  if(STN_UNDEF==symIdx)
  {
    death("could not reindex symbol from .o file to newBinary\n");
  }

  
  GElf_Sym symInNewBin;
  getSymbol(newBinary,symIdx,&symInNewBin);
  char* symname=elf_strptr(newBinary->e, newBinary->strTblIdx,
                           symInNewBin.st_name);
  //create a symbol to represent the symbol in newBinary
  //that this relocation refers to. If we don't already have a corresponding
  //symbol in the patch file, we may have to add this one
  Elf32_Sym sym;
  sym.st_name=addStrtabEntry(symname);
  sym.st_value=symInNewBin.st_value;
  sym.st_size=symInNewBin.st_size;
  sym.st_info=symInNewBin.st_info;
  int symType=ELF64_ST_TYPE(symInNewBin.st_info);
  sym.st_shndx=SHN_UNDEF;        
  if(STT_SECTION==symType)
  {
    sym.st_shndx=reindexSectionForPatch(newBinary,symInNewBin.st_shndx);
  }
  sym.st_info=ELF32_ST_INFO(ELF64_ST_BIND(symInNewBin.st_info),
                            symType);
  sym.st_other=symInNewBin.st_other;

  //we might have already added this same symbol to the patch
  GElf_Sym gsym=nativeSymToGELFSym(sym);
  int reindex=findSymbol(patch,&gsym,patch,false);
  if(SHN_UNDEF==reindex)
  {
    reindex=addSymtabEntry(getDataByERS(patch,ERS_SYMTAB),&sym);
  }
  return reindex;
}

//segmentBase should be the start of the text that these relocations refer to
void writeRelocationsInRange(addr_t lowpc,addr_t highpc,Elf_Scn* scn,
                             addr_t segmentBase,ElfInfo* binary)
{
  List* relocs=getRelocationItemsInRange(binary,scn,lowpc,highpc);
  List* li=relocs;
  idx_t rodataScnIdx=elf_ndxscn(getSectionByERS(binary,ERS_RODATA));//for special handling of rodata because lump rodata from several binaries into one section
  for(;li;li=li->next)
  {
    //we always use RELA rather than REL in the patch file
    //because having the addend recorded makes some
    //things much easier to work with
    Elf32_Rela rela;//what we actually write to the file
    RelocInfo* reloc=li->value;
    //todo: we insert symbols so that the relocations
    //will be valid, but we need to make sure we don't insert
    //a single symbol too many times, it wastes space
    int symIdx=reloc->symIdx;
    idx_t reindex=addSymbolFromBinaryToPatch(binary,symIdx);
    
    //now look at the relocation itself
    byte type=reloc->relocType;
    rela.r_offset=reloc->r_offset;
    //now rebase the offset based on where in the data this text is going
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    
    //note: this relies on our current incorrect usage of the offset field
    //for bookkeeping
    //todo: don't do that
    addr_t newRelOffset=rela.r_offset-lowpc+segmentBase;

    #ifdef legacy
    addr_t oldRelOffset=rela.r_offset;
    //now depending on the type we may have to do some additional fixups
    switch(type)
    {
      case R_386_PC32:
        {
          word_t addrAccessed=getWordAtAbs(elf_getscn(binary->e,reloc->scnIdx)
                                           ,oldRelOffset,IN_MEM);
          //this is a pc-relative relocation but we're changing the
          //address of the instruction. If we're not accessing
          //something inside the current function, then we might think
          //we actually want the type to be absolute, not relative. The problem
          //with that is often a R_386_PC32 deals with a value at r_offset
          //that is not an address itself but an offset from the current address
          //and we must keep it that way, for example for relative calls
          //therefore we try to fix things up as best we can, although
          //in many cases we'll be dealing with something in the PLT,
          //and it will get relocated properly for that symbol
          //and we can ignore most of the relocation entry anyway
          if(addrAccessed < lowpc || addrAccessed > highpc)
          {
            addr_t diff=newRelOffset-oldRelOffset;
            //that doesn't look right, look into this. Why are we writing it
            //setWordAtAbs(elf_getscn(binary->e,reloc->scnIdx),oldRelOffset,addrAccessed+diff,IN_MEM);
          
            //we're not just accessing something in the current function
            //tweak the addend so that even with the new value
            //we can access the same absolute address, which is what
            //we really want to do. The text segment will never get
            //relocated on linux x86 so the reasons for making this
            //relative in the first place are gone
            //rela.r_addend=addrAccessed-(newRelOffset+1)-sym.st_value;
          }
        }
        break;
    }
    #endif

    rela.r_info=ELF32_R_INFO(reindex,type);
    
    if(ERT_REL==reloc->type)
    {
      rela.r_addend=computeAddend(binary,type,symIdx,reloc->r_offset,reloc->scnIdx);
    }
    else
    {
      rela.r_addend=reloc->r_addend;
    }

    //special handling for rodata, because each object has its own rodata
    //and we're no lumping them all together, we have to increase the addend
    //to fit where rodata actually goes
    GElf_Sym sym;
    if(!gelf_getsym(getDataByERS(binary,ERS_SYMTAB),symIdx,&sym))
    {death("gelf_getsym failed in writeRelocationsInRange\n");}
    if(sym.st_shndx==rodataScnIdx)
    {
      rela.r_addend+=rodataOffset;//increase it by the amount we're up to

    }
    //end special handling for rodata

 
    rela.r_offset=newRelOffset;
    printf("adding reloc for offset 0x%x\n",rela.r_offset);
    addDataToScn(getDataByERS(patch,ERS_RELA_TEXT),&rela,sizeof(Elf32_Rela));
  }
  deleteList(relocs,free);
}


void writeCUToDwarf(CompilationUnit* cu)
{
  Dwarf_Error err;
  assert(!cu->die);
  cu->die=dwarf_new_die(dbg,DW_TAG_compile_unit,NULL,NULL,lastCUDie,NULL,&err);
  dwarf_add_AT_name(cu->die,cu->name,&err);
  if(!firstCUDie)
  {
    firstCUDie=cu->die;
  }
  lastCUDie=cu->die;
}


void writeFuncToDwarf(char* name,uint textOffset,uint funcTextSize,
                      int symIdx,CompilationUnit* cu)
{
  if(!cu->die)
  {
    writeCUToDwarf(cu);
  }
  Dwarf_Error err;
  Dwarf_P_Die parent=cu->lastDie?NULL:cu->die;
  assert(parent || cu->lastDie);
  Dwarf_P_Die die=dwarf_new_die(dbg,DW_TAG_subprogram,parent,NULL,cu->lastDie,NULL,&err);
  dwarf_add_AT_name(die,name,&err);
  //note that we add in highpc and lowpc info for the new version of
  //the function. This is so that when patching we can find it
  
  //todo: these are going to need relocation
  dwarf_add_AT_targ_address(dbg,die,DW_AT_low_pc,textOffset,symIdx,&err);
  dwarf_add_AT_targ_address(dbg,die,DW_AT_high_pc,textOffset+funcTextSize,symIdx,&err);
  cu->lastDie=die;
}

void writeTypeToDwarf(TypeInfo* type)
{
  if(!type->cu->die)
  {
    writeCUToDwarf(type->cu);
  }
  
  if(type->die)
  {
    return;//already written
  }
  int tag=0;
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
  Dwarf_P_Die parent=type->cu->lastDie?NULL:type->cu->die;
  assert(parent || type->cu->lastDie);
  Dwarf_Error err;
  Dwarf_P_Die die=dwarf_new_die(dbg,tag,parent,NULL,type->cu->lastDie,NULL,&err);
  type->cu->lastDie=die;
  type->die=die;
  dwarf_add_AT_name(die,type->name,&err);
  dwarf_add_AT_unsigned_const(dbg,die,DW_AT_byte_size,type->length,&err);
  if((TT_POINTER==type->type || TT_ARRAY==type->type) && TT_VOID!=type->pointedType->type)
  {
    if(strEndsWith(type->name,"~"))
    {
      //writing old type
      writeOldTypeToDwarf(type->pointedType,type->cu);
    }
    else
    {
      writeTypeToDwarf(type->pointedType);
    }
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
      if(strEndsWith(type->name,"~"))
      {
        writeOldTypeToDwarf(type->fieldTypes[i],type->cu);
      }
      else
      {
        writeTypeToDwarf(type->fieldTypes[i]);
      }
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


//generally we write only the new types,
//except we need the old types around for
//some things, such as figuring out the size
//of the old variable, which isn't always
//available (pointer issues)
//We note old types by appending a ~ to the name
void writeOldTypeToDwarf(TypeInfo* type,CompilationUnit* cuToWriteIn)
{
  CompilationUnit* oldCu=type->cu;
  type->cu=cuToWriteIn;
  char* oldName=type->name;
  type->name=zmalloc(strlen(type->name)+2);
  sprintf(type->name,"%s~",oldName);
  writeTypeToDwarf(type);//detects that we're doing an old type and takes care of the rest
  free(type->name);
  type->name=oldName;//restore the name
  type->cu=oldCu;

}


void writeVarToDwarf(VarInfo* var,CompilationUnit* cu)
{
  assert(cu);
  if(!cu->die)
  {
    writeCUToDwarf(cu);
  }
  Dwarf_Error err;
  writeTypeToDwarf(var->type);
  Dwarf_P_Die sibling=cu->lastDie;
  Dwarf_P_Die parent=NULL;
  if(!sibling)
  {
    parent=cu->die;
  }
  assert(parent || sibling);
  Dwarf_P_Die die=dwarf_new_die(dbg,DW_TAG_variable,parent,NULL,sibling,NULL,&err);
  dwarf_add_AT_name(die,var->name,&err);
  dwarf_add_AT_reference(dbg,die,DW_AT_type,var->type->die,&err);
  cu->lastDie=die;
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
  trans->fdeIdx=dwarf_add_frame_fde(dbg,fde,trans->to->die,cie,0,trans->to->length,0,&err);
  DwarfInstructions instrs;
  memset(&instrs,0,sizeof(DwarfInstructions));
  if(trans->straightCopy)
  {
    DwarfInstruction inst;
    inst.opcode=DW_CFA_register;
    byte bytes[1+sizeof(word_t)];
    bytes[0]=ERT_CURR_TARG_NEW;
    assert(trans->from->length==trans->to->length);
    memcpy(bytes+1,&trans->from->length,sizeof(word_t));
    //offset on the register is always 0, doing a complete copy of everything
    inst.arg1Bytes=encodeAsLEB128(bytes,1+sizeof(word_t),false,&inst.arg1NumBytes);
    bytes[0]=ERT_CURR_TARG_OLD;
    inst.arg2Bytes=encodeAsLEB128(bytes,1+sizeof(word_t),false,&inst.arg2NumBytes);
    addInstruction(&instrs,&inst);
    free(inst.arg1Bytes);
    free(inst.arg2Bytes);
  }
  else if(TT_POINTER==trans->from->type)
  {
    //then we're going to want one instruction, a recursive fixup
    DwarfInstruction inst;
    logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"adding pointer fixup fde\n");
    inst.opcode=DW_CFA_KATANA_fixups_pointer;
    assert(trans->from->pointedType);
    TypeTransform* transformer=trans->from->pointedType->transformer;
    assert(transformer);
    if(!transformer->onDisk)
    {
      writeTransformationToDwarf(transformer);
    }
    Dwarf_Unsigned fdeIdx=transformer->fdeIdx;
    byte bytes[1+sizeof(word_t)];
    bytes[0]=ERT_CURR_TARG_NEW;
    word_t size=sizeof(addr_t);
    memcpy(bytes+1,&size,sizeof(word_t));
    //offset on the register is always 0
    inst.arg1Bytes=encodeAsLEB128(bytes,1+sizeof(word_t),false,&inst.arg1NumBytes);
    bytes[0]=ERT_CURR_TARG_OLD;
    inst.arg2Bytes=encodeAsLEB128(bytes,1+sizeof(word_t),false,&inst.arg2NumBytes);
    inst.arg3=fdeIdx;//might as well make both valid
    inst.arg3Bytes=encodeAsLEB128((byte*)&fdeIdx,sizeof(fdeIdx),false,&inst.arg3NumBytes);
    addInstruction(&instrs,&inst);
    free(inst.arg1Bytes);
    free(inst.arg2Bytes);
    free(inst.arg3Bytes);
  }
  else //look at structure, not just straight copy
  {
    int oldBytesSoFar=0;
    for(int i=0;i<trans->from->numFields;i++)
    {
      DwarfInstruction inst;
      int off=trans->fieldOffsets[i];
      int size=trans->from->fieldTypes[i]->length;
      E_FIELD_TRANSFORM_TYPE transformType=trans->fieldTransformTypes[i];

      if(EFTT_DELETE==transformType)
      {
        logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"not addingfield to fde\n");
        oldBytesSoFar+=trans->from->fieldLengths[i];
        continue;
      }
#define NUM_BYTES 1+2*sizeof(word_t)
      byte bytes[NUM_BYTES];
      bytes[0]=ERT_CURR_TARG_NEW;
      memcpy(bytes+1,&size,sizeof(word_t));
      //todo: waste of space, whole point of LEB128, could put this in one
      //byte instead of 4 most of the time
      logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"field offset for field %i is %i\n",i,off);
      memcpy(bytes+1+sizeof(word_t),&off,sizeof(word_t));
      inst.arg1Bytes=encodeAsLEB128(bytes,NUM_BYTES,false,&inst.arg1NumBytes);
      bytes[0]=ERT_CURR_TARG_OLD;
      memcpy(bytes+1+sizeof(word_t),&oldBytesSoFar,sizeof(word_t));
      inst.arg2Bytes=encodeAsLEB128(bytes,NUM_BYTES,false,&inst.arg2NumBytes);
      if(EFTT_RECURSE==transformType)
      {
        logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"adding recurse field to fde\n");
        TypeInfo* fieldTypeFrom=trans->from->fieldTypes[i];
        TypeTransform* transformer=NULL;
        if(TT_POINTER==fieldTypeFrom->type)
        {
          inst.opcode=DW_CFA_KATANA_fixups_pointer;
          assert(fieldTypeFrom->pointedType);
          transformer=fieldTypeFrom->pointedType->transformer;
        }
        else
        {
          inst.opcode=DW_CFA_KATANA_fixups;
          transformer=fieldTypeFrom->transformer;
        }
        assert(transformer);
        if(!transformer->onDisk)
        {
          writeTransformationToDwarf(transformer);
        }
        Dwarf_Unsigned fdeIdx=transformer->fdeIdx;
        inst.arg3=fdeIdx;//might as well make both valid
        inst.arg3Bytes=encodeAsLEB128((byte*)&fdeIdx,sizeof(fdeIdx),false,&inst.arg3NumBytes);
        addInstruction(&instrs,&inst);
        free(inst.arg1Bytes);
        free(inst.arg2Bytes);
        free(inst.arg3Bytes);
        oldBytesSoFar+=trans->from->fieldLengths[i];
        continue;
      }
      logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"adding normal field to fde\n");
      //transforming a struct with fields that are base types, nice and easy
      inst.opcode=DW_CFA_register;
    
      addInstruction(&instrs,&inst);
      free(inst.arg1Bytes);
      free(inst.arg2Bytes);
      oldBytesSoFar+=trans->from->fieldLengths[i];

    }
  }
  printf("adding %i bytes to fde\n",instrs.numBytes);
  dwarf_insert_fde_inst_bytes(dbg,fde,instrs.numBytes,instrs.instrs,&err);
  free(instrs.instrs);
}

void writeVarToData(VarInfo* var)
{
  //todo: store address within var?
  int symIdx=getSymtabIdx(newBinary,var->name,0);
  if(STN_UNDEF==symIdx)
  {death("could not get symbol for var %s\n",var->name);}
  GElf_Sym sym;
  getSymbol(newBinary,symIdx,&sym);
  Elf_Scn* scn=elf_getscn(newBinary->e,sym.st_shndx);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {death("gelf_shdr failed\n");}
  byte* data=NULL;
  bool freeData=false;
  if(shdr.sh_type==SHT_PROGBITS)
  {
    data=getDataAtAbs(scn,sym.st_value,IN_MEM);
  }
  else if(shdr.sh_type==SHT_NOBITS)
  {
    data=zmalloc(var->type->length);
    freeData=true;
  }
  else
  {
    death("variable lives in unexpected section\n");
  }
  addr_t addr=addDataToScn(getDataByERS(patch,ERS_DATA),data,var->type->length);
  if(freeData)
  {
    free(data);
  }
  //now we must create a symbol for it as well
  Elf32_Sym symNew=gelfSymToNativeSym(sym);
  symNew.st_name=addStrtabEntry(var->name);
  symNew.st_value=addr;//will get relocated later once the data section in the patch has a fixed address
  symNew.st_shndx=elf_ndxscn(getSectionByERS(patch,ERS_DATA));
  addSymtabEntry(getDataByERS(patch,ERS_SYMTAB),&symNew);
}

//takes a list of var transformations and actually constructs the ELF sections
void writeVarTransforms(List* varTrans)
{
  List* li=varTrans;
  for(;li;li=li->next)
  {
    VarTransformation* vt=li->value;
    writeOldTypeToDwarf(vt->transform->from,vt->var->type->cu);
    writeVarToDwarf(vt->var,vt->cu);
    writeVarToData(vt->var);
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

void writeNewVarsForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  VarInfo** patchVars=(VarInfo**)dictValues(cuNew->tv->globalVars);
  Dictionary* oldVars=cuOld->tv->globalVars;
  VarInfo* var=patchVars[0];
  for(int i=0;var;i++,var=patchVars[i])
  {
    VarInfo* oldVar=dictGet(oldVars,var->name);
    if(!oldVar)
    {
      logprintf(ELL_INFO_V1,ELS_TYPEDIFF,"Found new variable %s\n",var->name);
      writeVarToDwarf(var,cuNew);
      writeVarToData(var);
    }
  }
  free(patchVars);
}

List* getTypeTransformationInfoForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  List* varTransHead=NULL;
  List* varTransTail=NULL;
  printf("Examining compilation unit %s\n",cuOld->name);
  VarInfo** vars1=(VarInfo**)dictValues(cuOld->tv->globalVars);

  VarInfo* var=vars1[0];
  Dictionary* patchVars=cuNew->tv->globalVars;
  for(int i=0;var;i++,var=vars1[i])
  {
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

    if(ti1->transformer)
    {
      needsTransform=true;
    }
    else
    {
      //todo: should have some sort of caching for types we've already
      //determined to be equal
      if(!compareTypesAndGenTransforms(ti1,ti2))
      {
        if(!ti1->transformer)
        {
          //todo: may not want to actually abort, may just want to issue
          //an error
          fprintf(stderr,"Error, cannot generate type transformation for variable %s\n",var->name);
          fflush(stderr);
          abort();
        }
        printf("generated type transformation for type %s\n",ti1->name);
        needsTransform=true;
      }
    }
    if(needsTransform)
    {
      List* li=zmalloc(sizeof(List));
      VarTransformation* vt=zmalloc(sizeof(VarTransformation));
      vt->var=patchedVar;
      vt->transform=var->type->transformer;
      vt->cu=cuNew;
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
  free(vars1);
  return varTransHead;
}



void writeFuncTransformationInfoForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  SubprogramInfo** funcs1=(SubprogramInfo**)dictValues(cuOld->subprograms);
  for(int i=0;funcs1[i];i++)
  {
    SubprogramInfo* func=funcs1[i];
    SubprogramInfo* patchedFunc=dictGet(cuNew->subprograms,func->name);
    if(!patchedFunc)
    {
      fprintf(stderr,"WARNING: function %s was removed from the patched version of compilation unit %s\n",func->name,cuNew->name);
      continue;
    }
    if(!areSubprogramsIdentical(func,patchedFunc,cuOld->elf,cuNew->elf))
    {
      printf("writing transformation info for function %s\n",func->name);
      int len=patchedFunc->highpc-patchedFunc->lowpc;
      Elf_Scn* textScn=getSectionByERS(patch,ERS_TEXT);
      Elf_Data* textData=getDataByERS(patch,ERS_TEXT);
      GElf_Shdr shdr;
      gelf_getshdr(textScn,&shdr);
      addr_t funcSegmentBase=textData->d_off+shdr.sh_addr;//where text for this function is based
      //if -ffunction-sections is used, the function might have its own text section
      Elf_Scn* textScnPatched=NULL;
      char buf[1024];
      snprintf(buf,1024,".text.%s",patchedFunc->name);
      textScnPatched=getSectionByName(cuNew->elf,buf);
      if(!textScnPatched)
      {
        textScnPatched=getSectionByERS(cuNew->elf,ERS_TEXT);
      }
      assert(textScnPatched);
      addDataToScn(textData,getDataAtAbs(textScnPatched,patchedFunc->lowpc,IN_MEM),len);
      Elf32_Sym sym;
      sym.st_name=addStrtabEntry(func->name);
      sym.st_value=textData->d_off;//is it ok that this is section-relative, since
      //we set st_shndx to be the text section
      sym.st_size=0;
      sym.st_info=ELF32_ST_INFO(STB_GLOBAL,STT_FUNC);
      sym.st_other=0;
      sym.st_shndx=elf_ndxscn(textScn);
      idx_t idx=addSymtabEntry(getDataByERS(patch,ERS_SYMTAB),&sym);

      //we also have to add an entry into debug info, so that
      //we know to patch the function
      writeFuncToDwarf(func->name,textData->d_off,len,idx,cuNew);
      textData->d_off+=len;

      //also include any relocations which exist for anything inside this function
      //if -ffunction-sections is specified,
      //each function will have its own relocation section,
      //check this out first
      snprintf(buf,1024,".rel.text.%s",patchedFunc->name);
      Elf_Scn* relocScn=getSectionByName(cuNew->elf,buf);
      if(!relocScn)
      {
        relocScn=getSectionByName(cuNew->elf,".rel.text");
      }
      if(!relocScn)
      {
        death("%s does not have a .rel.text section\n");
      }
      writeRelocationsInRange(patchedFunc->lowpc,patchedFunc->highpc,relocScn,funcSegmentBase,cuNew->elf);
    }
  }

  free(funcs1);
}

  
void writeTypeAndFuncTransformationInfo(ElfInfo* patchee,ElfInfo* patched)
{
  DwarfInfo* diPatchee=patchee->dwarfInfo;
  DwarfInfo* diPatched=patched->dwarfInfo;
  //  List* varTransHead=NULL;
  //  List* varTransTail=NULL;
  
      //todo: handle addition of variables and also handle
  //      things moving between compilation units,
  //      perhaps group global objects from all compilation units
  //      together before dealing with them.
  List* cuLi1=diPatchee->compilationUnits;
  for(;cuLi1;cuLi1=cuLi1->next)
  {
    CompilationUnit* cuOld=cuLi1->value;
    CompilationUnit* cuNew=NULL;
    //find the corresponding compilation unit in the patched process
    //note: we assume that the number of compilation units is not so large
    //that using a hash table would give us much better performance than
    //just going through a list. If working on a truly enormous project it
    //might make sense to do this
    List* cuLi2=diPatched->compilationUnits;
    for(;cuLi2;cuLi2=cuLi2->next)
    {
      cuNew=cuLi2->value;
      if(cuOld->name && cuNew->name && !strcmp(cuOld->name,cuNew->name))
      {
        break;
      }
      cuNew=NULL;
    }
    if(!cuNew)
    {
      fprintf(stderr,"WARNING: the patched version omits an entire compilation unit present in the original version.\n");
      if(cuOld->name)
      {
        fprintf(stderr,"Missing cu is named \"%s\"\n",cuOld->name);
      }
      else
      {
        fprintf(stderr,"The compilation unit in the patchee version does not have a name\n");
      }
      //todo: we also need to handle a compilation unit being present
      //in the patched version and not the patchee
      break;
    }
    cuOld->presentInOtherVersion=true;
    cuNew->presentInOtherVersion=true;
    List* li=getTypeTransformationInfoForCU(cuOld,cuNew);
    writeNewVarsForCU(cuOld,cuNew);
    writeVarTransforms(li);
    deleteList(li,free);
    
    //note that this must be done after dealing with the data
    //so that dealing with the data writes out the appropriate symbols
    writeFuncTransformationInfoForCU(cuOld,cuNew);


    printf("completed all transformations for compilation unit %s\n",cuOld->name);

  }
}

//takes an elf object that only has a pached version, no patchee version
//and writes it all out
void writeAllTypeAndFuncTransformationInfo(ElfInfo* elf)
{
  death("This function isn't written yet b/c James is an idiot\n");
}




//todo: in the future only copy rodata from object files which changed,
//don't necessarily need *all* the rodata
void writeROData(ElfInfo* binary)
{
  Elf_Data* roData=getDataByERS(binary,ERS_RODATA);
  Elf_Data* roDataPatch=getDataByERS(patch,ERS_RODATA);
  uint len=roData->d_size;
  rodataOffset=roDataPatch->d_size;//offset to put things off of now
  addDataToScn(roDataPatch,roData->d_buf,len);
}


void writePatch(char* oldSourceTree,char* newSourceTree,char* oldBinName,char* newBinName,char* patchOutName)
{
  newBinary=openELFFile(newBinName);
  findELFSections(newBinary);
  oldBinary=openELFFile(oldBinName);
  findELFSections(oldBinary);
  patch=startPatchElf(patchOutName);
  Dwarf_Error err;
  dbg=dwarf_producer_init(DW_DLC_WRITE,dwarfWriteSectionCallback,dwarfErrorHandler,NULL,&err);
  Dwarf_P_Die tmpRootDie=dwarf_new_die(dbg,DW_TAG_compile_unit,NULL,NULL,NULL,NULL,&err);
  dwarf_add_die_to_debug(dbg,tmpRootDie,&err);
  cie=dwarf_add_frame_cie(dbg,"",1,1,0,NULL,0,&err);
  if(DW_DLV_NOCOUNT==cie)
  {
    dwarfErrorHandler(err,"creating frame cie failed");
  }

  //todo: find better way of reording symbols so local ones all first
  //call this once so the necessary dwarf sections get created
  //dwarf_transform_to_disk_form(dbg,&err);

  //so far there should be only local sections in symtab. Can set sh_info
  //apropriately
  Elf32_Shdr* shdr=elf32_getshdr(getSectionByERS(patch,ERS_SYMTAB));
  shdr->sh_info=getDataByERS(patch,ERS_SYMTAB)->d_off/sizeof(Elf32_Sym)+1;
  printf("set symtab sh_info to %i\n",shdr->sh_info);
  
  //now that we've created the necessary things, actually run through
  //the stuff to write in our data
  List* objFiles=getChangedObjectFilesInSourceTree(oldSourceTree,newSourceTree);
  List* li=objFiles;
  for(;li;li=li->next)
  {
    ObjFileInfo* obj=li->value;
    switch(obj->state)
    {
    case EOS_MODIFIED:
      {
        ElfInfo* elf1=getOriginalObject(obj);
        readDWARFTypes(elf1);
        ElfInfo* elf2=getModifiedObject(obj);
        readDWARFTypes(elf2);
        //all the object files had their own roData sections
        //and now we're lumping them together
        writeROData(elf2);
        writeTypeAndFuncTransformationInfo(elf1,elf2);
        endELF(elf1);
        endELF(elf2);
      }
    break;
    case EOS_NEW:
      {
        ElfInfo* elf=getModifiedObject(obj);
        readDWARFTypes(elf);
        writeAllTypeAndFuncTransformationInfo(elf);
      }
    default:
      death("should only be seeing changed object files\n");
    }
  }
  deleteList(objFiles,(FreeFunc)deleteObjFileInfo);

  dwarf_add_die_to_debug(dbg,firstCUDie,&err);
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
  
  endPatchElf();
  int res=dwarf_producer_finish(dbg,&err);
  if(DW_DLV_ERROR==res)
  {
    dwarfErrorHandler(err,NULL);
  }
  printf("wrote elf file %s\n",patchOutName);
  endELF(oldBinary);
  endELF(newBinary);
}

