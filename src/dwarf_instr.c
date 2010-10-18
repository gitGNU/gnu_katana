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
    bytes[0]=DW_CFA_offset | instr->arg1;
    memcpy(bytes+1,instr->arg2Bytes,instr->arg2NumBytes);
    addBytes(instrs,bytes,1+instr->arg2NumBytes);
    break;
    //take care of all instructions taking one operand which is LEB128 or DW_FORM_block
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
    
    
    //location has no meaning (yet anyway) in patches
  case DW_CFA_set_loc:
  case DW_CFA_advance_loc:
  case DW_CFA_advance_loc1:
  case DW_CFA_advance_loc2:
  case DW_CFA_advance_loc4:
    //restore has no meaning in patches
  case DW_CFA_restore:
  case DW_CFA_restore_extended:
    //remember and restore state have no meaning
  case DW_CFA_remember_state:
  case DW_CFA_restore_state:
  default:
    {
      char buf[32];
      snprintf(buf,32,"Dwarf instruction with opcode %i not supported\n",instr->opcode);
      death(buf);
    }
    break;
  }
}

void printInstruction(RegInstruction inst)
{
  switch(inst.type)
  {
  case DW_CFA_set_loc:
    printf("DW_CFA_set_loc %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc:
    printf("DW_CFA_advance_loc %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc1:
    printf("DW_CFA_advance_loc_1 %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc2:
    printf("DW_CFA_advance_loc_2 %i\n",inst.arg1);
    break;
  case DW_CFA_offset:
    {
      signed long int tmp;
      memcpy(&tmp,&inst.arg2,sizeof(inst.arg2));
      printf("DW_CFA_offset_ r%i %zi\n",inst.arg1,tmp);
    }
    break;
  case DW_CFA_register:
    printf("DW_CFA_register ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i ",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf(" ");
    }
    if(ERT_NONE==inst.arg2Reg.type)
    {
      printf("r%lu ",(unsigned long)inst.arg2);
    }
    else
    {
      printReg(inst.arg2Reg,stdout);
      printf(" ");
    }
    printf("\n");
    break;
  case DW_CFA_KATANA_fixups:
    printf("DW_CFA_KATANA_fixups ");
  case DW_CFA_KATANA_fixups_pointer:
    {
      printf("DW_CFA_KATANA_fixups_pointer ");
    }
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i ",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf(" ");
    }
    if(ERT_NONE==inst.arg2Reg.type)
    {
      printf("r%lu ",(unsigned long)inst.arg2);
    }
    else
    {
      printReg(inst.arg2Reg,stdout);
      printf(" ");
    }
    printf("fde#%lu ",(unsigned long)inst.arg3);
    printf("\n");
    break;

  case DW_CFA_def_cfa:
    printf("DW_CFA_def_cfa ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i ",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf(" ");
    }
    printf("%li \n",(long)inst.arg2);
    break;
  case DW_CFA_def_cfa_register:
    printf("DW_CFA_def_cfa_register ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i\n",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf("\n");
    }
    break;
  case DW_CFA_def_cfa_offset:
    printf("DW_CFA_def_cfa_offset %i\n",inst.arg1);
    break;
  case DW_CFA_nop:
    printf("DW_CFA_nop\n");
    break;
  default:
    death("unsupported instruction");
  }
}
