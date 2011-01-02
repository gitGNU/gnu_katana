/*
  File: dwarf_instr.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

      The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: January 2010
  Description: Functions for manipulating dwarf instructions
*/

#include "util/util.h"
#include "dwarf_instr.h"
#include <math.h>
#include <dwarf.h>
#include "util/logging.h"
#include "leb.h"

char** dwarfExpressionNames;

void addBytes(DwarfInstructions* instrs,byte* bytes,int numBytes)
{
  if(instrs->allocated-instrs->numBytes < numBytes)
  {
    instrs->allocated=max(instrs->allocated*2,instrs->allocated+numBytes*2);
    instrs->instrs=realloc(instrs->instrs,instrs->allocated);
  }
  memcpy(instrs->instrs+instrs->numBytes,bytes,numBytes);
  instrs->numBytes+=numBytes;
}

//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr)
{
  byte b;
  byte* bytes;
  switch(instr->opcode)
  {
  case DW_CFA_nop:
  case DW_CFA_undefined://treat as a nop
  case DW_CFA_same_value://treat as a nop
    b=DW_CFA_nop;
    addBytes(instrs,&b,1);
    break;
  case DW_CFA_offset:
    bytes=zmalloc(1+instr->arg2NumBytes);
    assert(instr->arg1 < 128);
    bytes[0]=DW_CFA_offset | instr->arg1;
    memcpy(bytes+1,instr->arg2Bytes,instr->arg2NumBytes);
    addBytes(instrs,bytes,1+instr->arg2NumBytes);
    break;
    //take care of all instructions taking one operand which is LEB128 or DW_FORM_block
  case DW_CFA_restore_extended:
  case DW_CFA_def_cfa_register:
  case DW_CFA_def_cfa_offset:
  case DW_CFA_def_cfa_offset_sf:
  case DW_CFA_def_cfa_expression:
    bytes=zmalloc(1+instr->arg1NumBytes);
    bytes[0]=instr->opcode;
    memcpy(bytes+1,instr->arg1Bytes,instr->arg1NumBytes);
    addBytes(instrs,bytes,1+instr->arg1NumBytes);
    break;
    //we can take care of all instructions taking either two operands both of which
    //are either LEB128 or DW_FORM_block
  case DW_CFA_offset_extended:
#ifdef DW_CFA_offset_extended_sf //only available in Dwarf3
  case DW_CFA_offset_extended_sf:
#endif
  case DW_CFA_val_offset:
  case DW_CFA_val_offset_sf:
  case DW_CFA_register:
  case DW_CFA_val_expression:
  case DW_CFA_def_cfa:
  case DW_CFA_def_cfa_sf:
    bytes=zmalloc(1+instr->arg1NumBytes+instr->arg2NumBytes);
    bytes[0]=instr->opcode;
    memcpy(bytes+1,instr->arg1Bytes,instr->arg1NumBytes);
    memcpy(bytes+1+instr->arg1NumBytes,instr->arg2Bytes,instr->arg2NumBytes);
    addBytes(instrs,bytes,1+instr->arg1NumBytes+instr->arg2NumBytes);
    free(bytes);
    break;
  //we can take care of all instructions taking three operands all of which
  //are either LEB128 or DW_FORM_block
  case DW_CFA_KATANA_fixups:
  case DW_CFA_KATANA_fixups_pointer:
    bytes=zmalloc(1+instr->arg1NumBytes+instr->arg2NumBytes+instr->arg3NumBytes);
    bytes[0]=instr->opcode;
    memcpy(bytes+1,instr->arg1Bytes,instr->arg1NumBytes);
    memcpy(bytes+1+instr->arg1NumBytes,instr->arg2Bytes,instr->arg2NumBytes);
    memcpy(bytes+1+instr->arg1NumBytes+instr->arg2NumBytes,instr->arg3Bytes,
           instr->arg3NumBytes);
    addBytes(instrs,bytes,1+instr->arg1NumBytes+instr->arg2NumBytes+instr->arg3NumBytes);
    free(bytes);
    break;
    
  //location has no meaning (yet anyway) in patches. We do support more
  //general purpose dwarf manipulation though, so we'll support these
  //instructions for those purposes.
  case DW_CFA_set_loc:
    bytes=zmalloc(1+instr->arg1NumBytes);
    assert(instr->arg1NumBytes==sizeof(addr_t));
    bytes[0]=instr->opcode;
    memcpy(bytes+1,instr->arg1Bytes,instr->arg1NumBytes);
    addBytes(instrs,bytes,1+instr->arg1NumBytes);
    free(bytes);
    break;
  case DW_CFA_advance_loc:
    bytes=zmalloc(1);
    if(instr->arg1 > 0xc0)
    {
      //more than the low-order 6 bits are used
      death("Location will not fit into encoding for DW_CFA_advance_loc\n");
    }
    bytes[0]=instr->opcode | instr->arg1;
    addBytes(instrs,bytes,1);
    free(bytes);
    break;
  case DW_CFA_advance_loc1:
    bytes=zmalloc(2);
    if(instr->arg1 > 0xFF)
    {
      death("Location will not fit into encoding for DW_CFA_advance_loc1\n");
    }
    bytes[0]=instr->opcode;
    memcpy(bytes+1,&instr->arg1,1);
    addBytes(instrs,bytes,2);
    free(bytes);
    break;
  case DW_CFA_advance_loc2:
    bytes=zmalloc(3);
    if(instr->arg1 > 0xFFFF)
    {
      death("Location will not fit into encoding for DW_CFA_advance_loc\n");
    }
    bytes[0]=instr->opcode;
    memcpy(bytes+1,&instr->arg1,2);
    addBytes(instrs,bytes,3);
    free(bytes);
    break;
  case DW_CFA_advance_loc4:
    bytes=zmalloc(5);
    if(instr->arg1 > 0xFFFFFFFF)
    {
      death("Location will not fit into encoding for DW_CFA_advance_loc\n");
    }
    bytes[0]=instr->opcode;
    memcpy(bytes+1,&instr->arg1,4);
    addBytes(instrs,bytes,5);
    free(bytes);
    break;
  case DW_CFA_restore:
    bytes=zmalloc(1);
    bytes[0]=DW_CFA_restore | instr->arg1;
    memcpy(bytes+1,instr->arg2Bytes,instr->arg2NumBytes);
    addBytes(instrs,bytes,1);
    free(bytes);
    break;
  case DW_CFA_remember_state:
  case DW_CFA_restore_state:
    bytes=zmalloc(1);
    bytes[0]=instr->opcode;
    addBytes(instrs,bytes,1);
    free(bytes);
    break;
    break;
  default:
    {
      char buf[32];
      snprintf(buf,32,"Dwarf instruction with opcode %i not supported\n",instr->opcode);
      death(buf);
    }
    break;
  }
}


//printing flags should be OR'd DwarfInstructionPrintFlags
void printInstruction(FILE* file,RegInstruction inst,int printFlags)
{
  switch(inst.type)
  {
  case DW_CFA_set_loc:
    fprintf(file,"DW_CFA_set_loc %zi\n",inst.arg1);
    break;
  case DW_CFA_advance_loc:
    fprintf(file,"DW_CFA_advance_loc %zi\n",inst.arg1);
    break;
  case DW_CFA_advance_loc1:
    fprintf(file,"DW_CFA_advance_loc1 %zi\n",inst.arg1);
    break;
  case DW_CFA_advance_loc2:
    fprintf(file,"DW_CFA_advance_loc2 %zi\n",inst.arg1);
    break;
  case DW_CFA_offset:
    {
      signed long int tmp;
      memcpy(&tmp,&inst.arg2,sizeof(inst.arg2));
      fprintf(file,"DW_CFA_offset r%zi %zi\n",inst.arg1,tmp);
    }
    break;
  case DW_CFA_register:
    fprintf(file,"DW_CFA_register ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      fprintf(file,"r%zi ",inst.arg1);
    }
    else
    {
      printReg(file,inst.arg1Reg,printFlags);
      fprintf(file," ");
    }
    if(ERT_NONE==inst.arg2Reg.type)
    {
      fprintf(file,"r%lu ",(unsigned long)inst.arg2);
    }
    else
    {
      printReg(file,inst.arg2Reg,printFlags);
      fprintf(file," ");
    }
    fprintf(file,"\n");
    break;
  case DW_CFA_KATANA_fixups:
    fprintf(file,"DW_CFA_KATANA_fixups ");
  case DW_CFA_KATANA_fixups_pointer:
    {
      fprintf(file,"DW_CFA_KATANA_fixups_pointer ");
    }
    if(ERT_NONE==inst.arg1Reg.type)
    {
      fprintf(file,"r%zi ",inst.arg1);
    }
    else
    {
      printReg(file,inst.arg1Reg,printFlags);
      fprintf(file," ");
    }
    if(ERT_NONE==inst.arg2Reg.type)
    {
      fprintf(file,"r%lu ",(unsigned long)inst.arg2);
    }
    else
    {
      printReg(file,inst.arg2Reg,printFlags);
      fprintf(file," ");
    }
    fprintf(file,"fde#%lu ",(unsigned long)inst.arg3);
    fprintf(file,"\n");
    break;

  case DW_CFA_def_cfa:
    fprintf(file,"DW_CFA_def_cfa ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      fprintf(file,"r%zi ",inst.arg1);
    }
    else
    {
      printReg(file,inst.arg1Reg,printFlags);
      fprintf(file," ");
    }
    fprintf(file,"%li \n",(long)inst.arg2);
    break;
  case DW_CFA_def_cfa_register:
    fprintf(file,"DW_CFA_def_cfa_register ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      fprintf(file,"r%zi\n",inst.arg1);
    }
    else
    {
      printReg(file,inst.arg1Reg,printFlags);
      fprintf(file,"\n");
    }
    break;
  case DW_CFA_def_cfa_offset:
    fprintf(file,"DW_CFA_def_cfa_offset %zi\n",inst.arg1);
    break;
  case DW_CFA_remember_state:
    fprintf(file,"DW_CFA_remember_state\n");
    break;
  case DW_CFA_restore_state:
    fprintf(file,"DW_CFA_restore_state\n");
    break;
  case DW_CFA_nop:
    fprintf(file,"DW_CFA_nop\n");
    break;
  case DW_CFA_expression:
    fprintf(file,"DW_CFA_expression\n");
    printExpr(file,"\t\t",inst.expr,printFlags);
  default:
    death("unsupported DWARF instruction 0x%x",inst.type);
  }
}


DwarfInstruction regInstructionToRawDwarfInstruction(RegInstruction* inst)
{
  //this information is encoded following the table in section 7.23 of
  //the Dwarf v4 standard.
  DwarfInstruction result;
  memset(&result,0,sizeof(DwarfInstruction));
  result.opcode=inst->type;
  switch(inst->type)
  {
  case DW_CFA_set_loc:
    result.arg1=inst->arg1;
    break;
  case DW_CFA_advance_loc:
  case DW_CFA_advance_loc1:
  case DW_CFA_advance_loc2:
  case DW_CFA_advance_loc4:
    //the specifics of the length of the address will be taken care of
    //in addInstruction later
    result.arg1=inst->arg1;
    break;
  case DW_CFA_offset:
    //the specifics of encoding the first argument with the operand
    //will be taken care of in addInstruction later

    //todo: I don't think this LEB value is actually used, not sure we need it
    result.arg1Bytes=encodeRegAsLEB128(inst->arg1Reg,false,&result.arg1NumBytes);
    if(inst->arg1Reg.type==ERT_BASIC)
    {
      result.arg1=inst->arg1Reg.u.index;
    }
    else
    {
      death("Cannot use a non-basic register as an operation to DW_CFA_offset\n");
    }
    
    assert(result.arg1NumBytes==1);//since it's being encoded with the operand
    result.arg2=inst->arg2;
    result.arg2Bytes=intToLEB128(inst->arg2,&result.arg2NumBytes);
    break;
  case DW_CFA_register:
    result.arg1Bytes=encodeRegAsLEB128(inst->arg1Reg,false,&result.arg1NumBytes);
    result.arg2Bytes=encodeRegAsLEB128(inst->arg2Reg,false,&result.arg2NumBytes);
    break;
  case DW_CFA_offset_extended:
  case DW_CFA_def_cfa:
    result.arg1Bytes=encodeRegAsLEB128(inst->arg1Reg,false,&result.arg1NumBytes);
    result.arg2Bytes=encodeAsLEB128((byte*)&inst->arg2,sizeof(inst->arg2),false,&result.arg2NumBytes);
    break;
  case DW_CFA_def_cfa_register:
  case DW_CFA_restore_extended:
  case DW_CFA_undefined:
  case DW_CFA_same_value:
    result.arg1Bytes=encodeRegAsLEB128(inst->arg1Reg,false,&result.arg1NumBytes);
    break;
  case DW_CFA_def_cfa_offset:
    result.arg1Bytes=encodeAsLEB128((byte*)&inst->arg1,sizeof(inst->arg1),false,&result.arg1NumBytes);
    break;
  case DW_CFA_restore:
    result.arg1=inst->arg2;
  case DW_CFA_remember_state:
  case DW_CFA_restore_state:
  case DW_CFA_nop:
    //don't need any operands to these
    break;
  case DW_CFA_def_cfa_expression:
    //todo: technically this should be encoded as an exprloc rather than
    //DW_FORM_block but looking at the standard I can't actually see
    //any difference.... Both are an unsigned LEB length followed by length
    //data bytes.
    result.arg1Bytes=encodeDwarfExprAsFormBlock(inst->expr,&result.arg1NumBytes);
    break;
  case DW_CFA_expression:
  case DW_CFA_val_expression:
    result.arg1Bytes=encodeRegAsLEB128(inst->arg1Reg,false,&result.arg1NumBytes);
    result.arg2Bytes=encodeDwarfExprAsFormBlock(inst->expr,&result.arg2NumBytes);
    break;
  default:
    death("unsupported DWARF instruction 0x%x",inst->type);
  }
  return result;
}

//encodes a Dwarf Expression as a LEB-encoded length followed
//by length bytes of Dwarf expression
//the returned pointer should be freed
byte* encodeDwarfExprAsFormBlock(DwarfExpr expr,usint* numBytesOut)
{
  GrowingBuffer result;
  memset(&result,0,sizeof(result));
  for(int i=0;i<expr.numInstructions;i++)
  {
    DwarfExprInstr* instr=expr.instructions+i;
    //all expression instructions start with a 1-byte opcode
    addToGrowingBuffer(&result,&instr->type,1);
    switch(instr->type)
    {
      //all of the instructions for which we don't need to do anything else
    case DW_OP_lit0:
    case DW_OP_lit1:
    case DW_OP_lit2:
    case DW_OP_lit3:
    case DW_OP_lit4:
    case DW_OP_lit5:
    case DW_OP_lit6:
    case DW_OP_lit7:
    case DW_OP_lit8:
    case DW_OP_lit9:
    case DW_OP_lit10:
    case DW_OP_lit11:
    case DW_OP_lit12:
    case DW_OP_lit13:
    case DW_OP_lit14:
    case DW_OP_lit15:
    case DW_OP_lit16:
    case DW_OP_lit17:
    case DW_OP_lit18:
    case DW_OP_lit19:
    case DW_OP_lit20:
    case DW_OP_lit21:
    case DW_OP_lit22:
    case DW_OP_lit23:
    case DW_OP_lit24:
    case DW_OP_lit25:
    case DW_OP_lit26:
    case DW_OP_lit27:
    case DW_OP_lit28:
    case DW_OP_lit29:
    case DW_OP_lit30:
    case DW_OP_lit31:
    case DW_OP_dup:
    case DW_OP_drop:
    case DW_OP_over:
    case DW_OP_swap:
    case DW_OP_rot:
    case DW_OP_deref:
    case DW_OP_xderef:
    case DW_OP_push_object_address:
    case DW_OP_form_tls_address:
    case DW_OP_call_frame_cfa:
    case DW_OP_abs:
    case DW_OP_and:
    case DW_OP_div:
    case DW_OP_minus:
    case DW_OP_mod:
    case DW_OP_mul:
    case DW_OP_neg:
    case DW_OP_not:
    case DW_OP_or:
    case DW_OP_plus:
    case DW_OP_shl:
    case DW_OP_shr:
    case DW_OP_shra:
    case DW_OP_xor:
    case DW_OP_le:
    case DW_OP_ge:
    case DW_OP_eq:
    case DW_OP_lt:
    case DW_OP_gt:
    case DW_OP_ne:
    case DW_OP_nop:
      //don't need to do anything for these opcodes
      break;
    //handle all the opcodes which take a 1-byte argument
    case DW_OP_const1u:
    case DW_OP_const1s:
    case DW_OP_pick:
    case DW_OP_deref_size:
    case DW_OP_xderef_size:
      addToGrowingBuffer(&result,&instr->arg1,1);
      break;
    //handle all the opcodes which take a 2-byte argument
    case DW_OP_const2u:
    case DW_OP_const2s:
    case DW_OP_skip:
    case DW_OP_bra:
      addToGrowingBuffer(&result,&instr->arg1,2);
      break;
    //handle all the opcodes which take a 4-byte argument
    case DW_OP_const4u:
    case DW_OP_const4s:
      addToGrowingBuffer(&result,&instr->arg1,4);
      break;
    //handle all the opcodes which take an 8-byte arugment
    case DW_OP_const8u:
    case DW_OP_const8s:
      //this may be an issue if on 32-bit but presumably these don't
      //get used on 32-bit. We'll add support to katana for it if we
      //need to
      assert(sizeof(instr->arg1)>=8);
      addToGrowingBuffer(&result,&instr->arg1,8);
      break;
    //handle all the opcodes which take a target machine address
    //sized argument
    case DW_OP_addr:
      addToGrowingBuffer(&result,&instr->arg1,sizeof(addr_t));
      break;
    //handle all the opcodes which take an unsigned LEB argument
    case DW_OP_constu:
    case DW_OP_plus_uconst:
      {
        usint numBytes;
        byte* data=encodeAsLEB128((byte*)&instr->arg1,sizeof(instr->arg1),false,&numBytes);
        addToGrowingBuffer(&result,data,numBytes);
        free(data);
      }
      break;
    //handle all the opcodes which take a signed LEB argument
    case DW_OP_consts:
    case DW_OP_fbreg:
    case DW_OP_breg0:
    case DW_OP_breg1:
    case DW_OP_breg2:
    case DW_OP_breg3:
    case DW_OP_breg4:
    case DW_OP_breg5:
    case DW_OP_breg6:
    case DW_OP_breg7:
    case DW_OP_breg8:
    case DW_OP_breg9:
    case DW_OP_breg10:
    case DW_OP_breg11:
    case DW_OP_breg12:
    case DW_OP_breg13:
    case DW_OP_breg14:
    case DW_OP_breg15:
    case DW_OP_breg16:
    case DW_OP_breg17:
    case DW_OP_breg18:
    case DW_OP_breg19:
    case DW_OP_breg20:
    case DW_OP_breg21:
    case DW_OP_breg22:
    case DW_OP_breg23:
    case DW_OP_breg24:
    case DW_OP_breg25:
    case DW_OP_breg26:
    case DW_OP_breg27:
    case DW_OP_breg28:
    case DW_OP_breg29:
    case DW_OP_breg30:
    case DW_OP_breg31:
      {
        usint numBytes;
        byte* data=encodeAsLEB128((byte*)&instr->arg1,sizeof(instr->arg1),true,&numBytes);
        addToGrowingBuffer(&result,data,numBytes);
        free(data);
      }
      break;
    //instructions with more than one argument
    case DW_OP_bregx:
      {
        usint numBytes;
        byte* data=encodeAsLEB128((byte*)&instr->arg1,sizeof(instr->arg1),false,&numBytes);
        addToGrowingBuffer(&result,data,numBytes);
        free(data);
        data=encodeAsLEB128((byte*)&instr->arg2,sizeof(instr->arg2),true,&numBytes);
        addToGrowingBuffer(&result,data,numBytes);
        free(data);
      }
      break;
    default:
      death("Unsupported DW_OP with code 0x%x\n",instr->type);
    }
  }
  *numBytesOut=result.len;
  return result.data;
}

//convert the higher-level RegInstruction format into the raw string
//of binary bytes that is the DwarfInstructions structure
DwarfInstructions serializeDwarfRegInstructions(RegInstruction* regInstrs,int numRegInstrs)
{
  DwarfInstructions result;
  memset(&result,0,sizeof(result));
  for(int i=0;i<numRegInstrs;i++)
  {
    DwarfInstruction instr=regInstructionToRawDwarfInstruction(regInstrs+i);
    addInstruction(&result,&instr);
  }
  return result;
}

void destroyRawInstructions(DwarfInstructions instrs)
{
  free(instrs.instrs);
}

void addToDwarfExpression(DwarfExpr* expr,DwarfExprInstr instr)
{
  expr->instructions=realloc(expr->instructions,expr->numInstructions+1);
  expr->instructions[expr->numInstructions]=instr;
  expr->numInstructions++;
}

void initDwarfExpressionNames()
{
  dwarfExpressionNames=zmalloc(sizeof(char*)*DW_OP_hi_user);
  dwarfExpressionNames[DW_OP_addr]="DW_OP_addr";
  dwarfExpressionNames[DW_OP_deref]="DW_OP_deref";
  dwarfExpressionNames[DW_OP_const1u]="DW_OP_const1u";
  dwarfExpressionNames[DW_OP_const1s]="DW_OP_const1s";
  dwarfExpressionNames[DW_OP_const2u]="DW_OP_const2u";
  dwarfExpressionNames[DW_OP_const2s]="DW_OP_const2s";
  dwarfExpressionNames[DW_OP_const4u]="DW_OP_const4u";
  dwarfExpressionNames[DW_OP_const4s]="DW_OP_const4s";
  dwarfExpressionNames[DW_OP_const8u]="DW_OP_const8u";
  dwarfExpressionNames[DW_OP_const8s]="DW_OP_const8s";
  dwarfExpressionNames[DW_OP_constu]="DW_OP_constu";
  dwarfExpressionNames[DW_OP_consts]="DW_OP_consts";
  dwarfExpressionNames[DW_OP_dup]="DW_OP_dup";
  dwarfExpressionNames[DW_OP_drop]="DW_OP_drop";
  dwarfExpressionNames[DW_OP_over]="DW_OP_over";
  dwarfExpressionNames[DW_OP_pick]="DW_OP_pick";
  dwarfExpressionNames[DW_OP_swap]="DW_OP_swap";
  dwarfExpressionNames[DW_OP_rot]="DW_OP_rot";
  dwarfExpressionNames[DW_OP_xderef]="DW_OP_xderef";
  dwarfExpressionNames[DW_OP_abs]="DW_OP_abs";
  dwarfExpressionNames[DW_OP_and]="DW_OP_and";
  dwarfExpressionNames[DW_OP_div]="DW_OP_div";
  dwarfExpressionNames[DW_OP_minus]="DW_OP_minus";
  dwarfExpressionNames[DW_OP_mod]="DW_OP_mod";
  dwarfExpressionNames[DW_OP_mul]="DW_OP_mul";
  dwarfExpressionNames[DW_OP_neg]="DW_OP_neg";
  dwarfExpressionNames[DW_OP_not]="DW_OP_not";
  dwarfExpressionNames[DW_OP_or]="DW_OP_or";
  dwarfExpressionNames[DW_OP_plus]="DW_OP_plus";
  dwarfExpressionNames[DW_OP_plus_uconst]="DW_OP_plus_uconst";
  dwarfExpressionNames[DW_OP_shl]="DW_OP_shl";
  dwarfExpressionNames[DW_OP_shr]="DW_OP_shr";
  dwarfExpressionNames[DW_OP_shra]="DW_OP_shra";
  dwarfExpressionNames[DW_OP_xor]="DW_OP_xor";
  dwarfExpressionNames[DW_OP_bra]="DW_OP_bra";
  dwarfExpressionNames[DW_OP_eq]="DW_OP_eq";
  dwarfExpressionNames[DW_OP_ge]="DW_OP_ge";
  dwarfExpressionNames[DW_OP_gt]="DW_OP_gt";
  dwarfExpressionNames[DW_OP_le]="DW_OP_le";
  dwarfExpressionNames[DW_OP_lt]="DW_OP_lt";
  dwarfExpressionNames[DW_OP_ne]="DW_OP_ne";
  dwarfExpressionNames[DW_OP_skip]="DW_OP_skip";
  dwarfExpressionNames[DW_OP_lit0]="DW_OP_lit0";
  dwarfExpressionNames[DW_OP_lit1]="DW_OP_lit1";
  dwarfExpressionNames[DW_OP_lit2]="DW_OP_lit2";
  dwarfExpressionNames[DW_OP_lit3]="DW_OP_lit3";
  dwarfExpressionNames[DW_OP_lit4]="DW_OP_lit4";
  dwarfExpressionNames[DW_OP_lit5]="DW_OP_lit5";
  dwarfExpressionNames[DW_OP_lit6]="DW_OP_lit6";
  dwarfExpressionNames[DW_OP_lit7]="DW_OP_lit7";
  dwarfExpressionNames[DW_OP_lit8]="DW_OP_lit8";
  dwarfExpressionNames[DW_OP_lit9]="DW_OP_lit9";
  dwarfExpressionNames[DW_OP_lit10]="DW_OP_lit10";
  dwarfExpressionNames[DW_OP_lit11]="DW_OP_lit11";
  dwarfExpressionNames[DW_OP_lit12]="DW_OP_lit12";
  dwarfExpressionNames[DW_OP_lit13]="DW_OP_lit13";
  dwarfExpressionNames[DW_OP_lit14]="DW_OP_lit14";
  dwarfExpressionNames[DW_OP_lit15]="DW_OP_lit15";
  dwarfExpressionNames[DW_OP_lit16]="DW_OP_lit16";
  dwarfExpressionNames[DW_OP_lit17]="DW_OP_lit17";
  dwarfExpressionNames[DW_OP_lit18]="DW_OP_lit18";
  dwarfExpressionNames[DW_OP_lit19]="DW_OP_lit19";
  dwarfExpressionNames[DW_OP_lit20]="DW_OP_lit20";
  dwarfExpressionNames[DW_OP_lit21]="DW_OP_lit21";
  dwarfExpressionNames[DW_OP_lit22]="DW_OP_lit22";
  dwarfExpressionNames[DW_OP_lit23]="DW_OP_lit23";
  dwarfExpressionNames[DW_OP_lit24]="DW_OP_lit24";
  dwarfExpressionNames[DW_OP_lit25]="DW_OP_lit25";
  dwarfExpressionNames[DW_OP_lit26]="DW_OP_lit26";
  dwarfExpressionNames[DW_OP_lit27]="DW_OP_lit27";
  dwarfExpressionNames[DW_OP_lit28]="DW_OP_lit28";
  dwarfExpressionNames[DW_OP_lit29]="DW_OP_lit29";
  dwarfExpressionNames[DW_OP_lit30]="DW_OP_lit30";
  dwarfExpressionNames[DW_OP_lit31]="DW_OP_lit31";
  dwarfExpressionNames[DW_OP_reg0]="DW_OP_reg0";
  dwarfExpressionNames[DW_OP_reg1]="DW_OP_reg1";
  dwarfExpressionNames[DW_OP_reg2]="DW_OP_reg2";
  dwarfExpressionNames[DW_OP_reg3]="DW_OP_reg3";
  dwarfExpressionNames[DW_OP_reg4]="DW_OP_reg4";
  dwarfExpressionNames[DW_OP_reg5]="DW_OP_reg5";
  dwarfExpressionNames[DW_OP_reg6]="DW_OP_reg6";
  dwarfExpressionNames[DW_OP_reg7]="DW_OP_reg7";
  dwarfExpressionNames[DW_OP_reg8]="DW_OP_reg8";
  dwarfExpressionNames[DW_OP_reg9]="DW_OP_reg9";
  dwarfExpressionNames[DW_OP_reg10]="DW_OP_reg10";
  dwarfExpressionNames[DW_OP_reg11]="DW_OP_reg11";
  dwarfExpressionNames[DW_OP_reg12]="DW_OP_reg12";
  dwarfExpressionNames[DW_OP_reg13]="DW_OP_reg13";
  dwarfExpressionNames[DW_OP_reg14]="DW_OP_reg14";
  dwarfExpressionNames[DW_OP_reg15]="DW_OP_reg15";
  dwarfExpressionNames[DW_OP_reg16]="DW_OP_reg16";
  dwarfExpressionNames[DW_OP_reg17]="DW_OP_reg17";
  dwarfExpressionNames[DW_OP_reg18]="DW_OP_reg18";
  dwarfExpressionNames[DW_OP_reg19]="DW_OP_reg19";
  dwarfExpressionNames[DW_OP_reg20]="DW_OP_reg20";
  dwarfExpressionNames[DW_OP_reg21]="DW_OP_reg21";
  dwarfExpressionNames[DW_OP_reg22]="DW_OP_reg22";
  dwarfExpressionNames[DW_OP_reg23]="DW_OP_reg23";
  dwarfExpressionNames[DW_OP_reg24]="DW_OP_reg24";
  dwarfExpressionNames[DW_OP_reg25]="DW_OP_reg25";
  dwarfExpressionNames[DW_OP_reg26]="DW_OP_reg26";
  dwarfExpressionNames[DW_OP_reg27]="DW_OP_reg27";
  dwarfExpressionNames[DW_OP_reg28]="DW_OP_reg28";
  dwarfExpressionNames[DW_OP_reg29]="DW_OP_reg29";
  dwarfExpressionNames[DW_OP_reg30]="DW_OP_reg30";
  dwarfExpressionNames[DW_OP_reg31]="DW_OP_reg31";
  dwarfExpressionNames[DW_OP_breg0]="DW_OP_breg0";
  dwarfExpressionNames[DW_OP_breg1]="DW_OP_breg1";
  dwarfExpressionNames[DW_OP_breg2]="DW_OP_breg2";
  dwarfExpressionNames[DW_OP_breg3]="DW_OP_breg3";
  dwarfExpressionNames[DW_OP_breg4]="DW_OP_breg4";
  dwarfExpressionNames[DW_OP_breg5]="DW_OP_breg5";
  dwarfExpressionNames[DW_OP_breg6]="DW_OP_breg6";
  dwarfExpressionNames[DW_OP_breg7]="DW_OP_breg7";
  dwarfExpressionNames[DW_OP_breg8]="DW_OP_breg8";
  dwarfExpressionNames[DW_OP_breg9]="DW_OP_breg9";
  dwarfExpressionNames[DW_OP_breg10]="DW_OP_breg10";
  dwarfExpressionNames[DW_OP_breg11]="DW_OP_breg11";
  dwarfExpressionNames[DW_OP_breg12]="DW_OP_breg12";
  dwarfExpressionNames[DW_OP_breg13]="DW_OP_breg13";
  dwarfExpressionNames[DW_OP_breg14]="DW_OP_breg14";
  dwarfExpressionNames[DW_OP_breg15]="DW_OP_breg15";
  dwarfExpressionNames[DW_OP_breg16]="DW_OP_breg16";
  dwarfExpressionNames[DW_OP_breg17]="DW_OP_breg17";
  dwarfExpressionNames[DW_OP_breg18]="DW_OP_breg18";
  dwarfExpressionNames[DW_OP_breg19]="DW_OP_breg19";
  dwarfExpressionNames[DW_OP_breg20]="DW_OP_breg20";
  dwarfExpressionNames[DW_OP_breg21]="DW_OP_breg21";
  dwarfExpressionNames[DW_OP_breg22]="DW_OP_breg22";
  dwarfExpressionNames[DW_OP_breg23]="DW_OP_breg23";
  dwarfExpressionNames[DW_OP_breg24]="DW_OP_breg24";
  dwarfExpressionNames[DW_OP_breg25]="DW_OP_breg25";
  dwarfExpressionNames[DW_OP_breg26]="DW_OP_breg26";
  dwarfExpressionNames[DW_OP_breg27]="DW_OP_breg27";
  dwarfExpressionNames[DW_OP_breg28]="DW_OP_breg28";
  dwarfExpressionNames[DW_OP_breg29]="DW_OP_breg29";
  dwarfExpressionNames[DW_OP_breg30]="DW_OP_breg30";
  dwarfExpressionNames[DW_OP_breg31]="DW_OP_breg31";
  dwarfExpressionNames[DW_OP_regx]="DW_OP_regx";
  dwarfExpressionNames[DW_OP_fbreg]="DW_OP_fbreg";
  dwarfExpressionNames[DW_OP_bregx]="DW_OP_bregx";
  dwarfExpressionNames[DW_OP_piece]="DW_OP_piece";
  dwarfExpressionNames[DW_OP_deref_size]="DW_OP_deref_size";
  dwarfExpressionNames[DW_OP_xderef_size]="DW_OP_xderef_size";
  dwarfExpressionNames[DW_OP_nop]="DW_OP_nop";
  dwarfExpressionNames[DW_OP_push_object_address]="DW_OP_push_object_address";
  dwarfExpressionNames[DW_OP_call2]="DW_OP_call2";
  dwarfExpressionNames[DW_OP_call4]="DW_OP_call4";
  dwarfExpressionNames[DW_OP_call_ref]="DW_OP_call_ref";
  dwarfExpressionNames[DW_OP_form_tls_address]="DW_OP_form_tls_address";
  dwarfExpressionNames[DW_OP_call_frame_cfa]="DW_OP_call_frame_cfa";
  dwarfExpressionNames[DW_OP_bit_piece]="DW_OP_bit_piece";
  dwarfExpressionNames[DW_OP_implicit_value]="DW_OP_implicit_value";
  dwarfExpressionNames[DW_OP_stack_value]="DW_OP_stack_value";

}


//printFlags should be OR'd DwarfInstructionPrintFlags
void printExprInstruction(FILE* file,char* prefix,DwarfExprInstr instr,int printFlags)
{
  if(!dwarfExpressionNames)
  {
    initDwarfExpressionNames();
  }
  assert(instr.type<DW_OP_hi_user);
  if(prefix)
  {
    fprintf(file,"%s",prefix);
  }
  fprintf(file,"%s",dwarfExpressionNames[instr.type]);
  
  //a lot of Dwarf Expression instructions don't have any operands
  //for the ones that do, however, we need to print out those operands
  switch(instr.type)
  {
    //first deal with the unsigned ones
  case DW_OP_const1u:
  case DW_OP_pick:
  case DW_OP_deref_size:
  case DW_OP_xderef_size:
  case DW_OP_const2u:
  case DW_OP_skip:
  case DW_OP_bra:
  case DW_OP_const4u:
  case DW_OP_const8u:
  case DW_OP_addr:
  case DW_OP_constu:
  case DW_OP_plus_uconst:
    fprintf(file," 0x%zx",instr.arg1);
    break;
    //then deal with the signed ones
  case DW_OP_const1s:
  case DW_OP_const2s:
  case DW_OP_const4s:
  case DW_OP_const8s:
  case DW_OP_consts:
  case DW_OP_breg0:
  case DW_OP_breg1:
  case DW_OP_breg2:
  case DW_OP_breg3:
  case DW_OP_breg4:
  case DW_OP_breg5:
  case DW_OP_breg6:
  case DW_OP_breg7:
  case DW_OP_breg8:
  case DW_OP_breg9:
  case DW_OP_breg10:
  case DW_OP_breg11:
  case DW_OP_breg12:
  case DW_OP_breg13:
  case DW_OP_breg14:
  case DW_OP_breg15:
  case DW_OP_breg16:
  case DW_OP_breg17:
  case DW_OP_breg18:
  case DW_OP_breg19:
  case DW_OP_breg20:
  case DW_OP_breg21:
  case DW_OP_breg22:
  case DW_OP_breg23:
  case DW_OP_breg24:
  case DW_OP_breg25:
  case DW_OP_breg26:
  case DW_OP_breg27:
  case DW_OP_breg28:
  case DW_OP_breg29:
  case DW_OP_breg30:
  case DW_OP_breg31:
  case DW_OP_fbreg:
    fprintf(file," 0x%zi",(sword_t)instr.arg1);
    break;
  //instructions with more than one argument
  case DW_OP_bregx:
    fprintf(file," 0x%zx",instr.arg1);
    fprintf(file," 0x%zi",(sword_t)instr.arg2);
    break;
  }
  fprintf(file,"\n");
}


void printExpr(FILE* file,char* prefix,DwarfExpr expr,int printFlags)
{
  for(int i=0;i<expr.numInstructions;i++)
  {
    printExprInstruction(file,prefix,expr.instructions[i],printFlags);
  }
}
