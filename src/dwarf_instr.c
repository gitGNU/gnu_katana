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
  default:
    death("unsupported DWARF instruction 0x%x",inst->type);
  }
  return result;
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
