/*
  File: write_to_dwarf.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: methods for writing the dwarf information part of the patch
*/

#include "types.h"
#include <libdwarf.h>
#include <dwarf.h>
#include "dwarf_instr.h"
#include "util/logging.h"

//todo: using externs is poor form, fix this
extern Dwarf_P_Die firstCUDie;
extern Dwarf_P_Die lastCUDie;//keep track of for sibling purposes
extern Dwarf_Unsigned cie;//index of cie we're using
void writeOldTypeToDwarf(Dwarf_P_Debug dbg,TypeInfo* type,CompilationUnit* cuToWriteIn);

void writeCUToDwarf(Dwarf_P_Debug dbg,CompilationUnit* cu)
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

void writeFuncToDwarf(Dwarf_P_Debug dbg,char* name,uint textOffset,uint funcTextSize,
                      int symIdx,CompilationUnit* cu,bool new)
{
  if(!cu->die)
  {
    writeCUToDwarf(dbg,cu);
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
  if(new)
  {
    dwarf_add_AT_flag(dbg,die,DW_AT_allocated,0,&err);
  }
  cu->lastDie=die;
}

void writeTypeToDwarf(Dwarf_P_Debug dbg,TypeInfo* type)
{
  if(!type->cu->die)
  {
    writeCUToDwarf(dbg,type->cu);
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
  case TT_UNION:
    tag=DW_TAG_union_type;
    break;
  case TT_ENUM:
    tag=DW_TAG_enumeration_type;
    break;
  case TT_VOID:
    return;//don't actually need to write out the void type, DWARF doesn't represent it
    break;
  default:
    death("TypeInfo has unknown type\n");
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
      writeOldTypeToDwarf(dbg,type->pointedType,type->cu);
    }
    else
    {
      writeTypeToDwarf(dbg,type->pointedType);
    }
    dwarf_add_AT_reference(dbg,die,DW_AT_type,type->pointedType->die,&err);
  }
  if(TT_ARRAY==type->type)
  {
    dwarf_add_AT_signed_const(dbg,die,DW_AT_lower_bound,type->lowerBound,&err);
    dwarf_add_AT_signed_const(dbg,die,DW_AT_upper_bound,type->upperBound,&err);
  }
  else if(TT_STRUCT==type->type || TT_UNION==type->type)
  {
    Dwarf_P_Die lastMemberDie=NULL;
    for(int i=0;i<type->numFields;i++)
    {
      if(strEndsWith(type->name,"~"))
      {
        writeOldTypeToDwarf(dbg,type->fieldTypes[i],type->cu);
      }
      else
      {
        writeTypeToDwarf(dbg,type->fieldTypes[i]);
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
void writeOldTypeToDwarf(Dwarf_P_Debug dbg,TypeInfo* type,CompilationUnit* cuToWriteIn)
{
  CompilationUnit* oldCu=type->cu;
  type->cu=cuToWriteIn;
  char* oldName=type->name;
  type->name=zmalloc(strlen(type->name)+2);
  sprintf(type->name,"%s~",oldName);
  writeTypeToDwarf(dbg,type);//detects that we're doing an old type and takes care of the rest
  free(type->name);
  type->name=oldName;//restore the name
  type->cu=oldCu;

}

void writeVarToDwarf(Dwarf_P_Debug dbg,VarInfo* var,CompilationUnit* cu,bool new)
{
  assert(cu);
  if(!cu->die)
  {
    writeCUToDwarf(dbg,cu);
  }
  Dwarf_Error err;
  writeTypeToDwarf(dbg,var->type);
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
  if(new)
  {
    dwarf_add_AT_flag(dbg,die,DW_AT_allocated,0,&err);
  }
  cu->lastDie=die;
}

void writeTransformationToDwarf(Dwarf_P_Debug dbg,TypeTransform* trans)
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
    assert(trans->from->length<=trans->to->length);
    memcpy(bytes+1,&trans->to->length,sizeof(word_t));
    //offset on the register is always 0, doing a complete copy of everything
    inst.arg1Bytes=encodeAsLEB128(bytes,1+sizeof(word_t),false,&inst.arg1NumBytes);
    bytes[0]=ERT_CURR_TARG_OLD;
    //todo: should we just use the from length for everything, since it's the
    //shorter of the two
    memcpy(bytes+1,&trans->from->length,sizeof(word_t));
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
      writeTransformationToDwarf(dbg,transformer);
    }
    idx_t fdeIdx=transformer->fdeIdx;
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
  else if(TT_STRUCT==trans->from->type) //look at structure, not just straight copy
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
          writeTransformationToDwarf(dbg,transformer);
        }
        idx_t fdeIdx=transformer->fdeIdx;
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
  else
  {
    death("unsupported TypeInfo type in writeTransformationToDwarf\n");
  }
  printf("adding %i bytes to fde\n",instrs.numBytes);
  dwarf_insert_fde_inst_bytes(dbg,fde,instrs.numBytes,instrs.instrs,&err);
  free(instrs.instrs);
}
