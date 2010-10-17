/*
  File: fderead.c
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
  Date: February 2010
  Description: Read the FDE's from the section formatted as a .debug_frame section.
               These FDEs contain instructions used for patching the structure  
*/

#include "dwarftypes.h"
#include "fderead.h"
#include "register.h"
#include "dwarf_instr.h"
#include <dwarf.h>
#include <assert.h>
#include "util/logging.h"
#include "dwarfvm.h"





//the returned memory should be freed
RegInstruction* parseFDEInstructions(Dwarf_Debug dbg,unsigned char* bytes,int len,
                                     int dataAlign,int codeAlign,int* numInstrs)
{
  *numInstrs=0;
  //allocate more mem than we'll actually need
  //we can free some later
  RegInstruction* result=zmalloc(sizeof(RegInstruction)*len);
  for(;len>0;len--,bytes++,(*numInstrs)++)
  {
    //as dwarfdump does, separate out high and low portions
    //of the byte based on the boundaries of the instructions that
    //encode an operand with the instruction
    //all other instructions are accounted for only by the bottom part of the byte
    unsigned char byte=*bytes;
    int high = byte & 0xc0;
    int low = byte & 0x3f;
    short unsigned int uleblen;
    result[*numInstrs].arg1Reg.type=ERT_NONE;//not using reg unless we set its type
    result[*numInstrs].arg2Reg.type=ERT_NONE;//not using reg unless we set its type
    switch(high)
    {
    case DW_CFA_advance_loc:
      result[*numInstrs].type=high;
      result[*numInstrs].arg1=low*codeAlign;
      break;
    case DW_CFA_offset:
      result[*numInstrs].type=high;
      result[*numInstrs].arg1=low;
      result[*numInstrs].arg1Reg.type=ERT_BASIC;
      result[*numInstrs].arg1Reg.u.index=low;
      result[*numInstrs].arg2=leb128ToUInt(bytes + 1, &uleblen)*dataAlign;
      bytes+=uleblen;
      len-=uleblen;
      break;
    case DW_CFA_restore:
      death("DW_CFA_restore not handled\n");
      break;
    default:
      //deal with the things encoded by the bottom portion
      result[*numInstrs].type=low;
      switch(low)
      {
      case DW_CFA_set_loc:
        //todo: this assumes 32-bit
        //does it? need to look into this more
        memcpy(&result[*numInstrs].arg1,bytes+1,sizeof(int));
        bytes+=sizeof(int);
        len-=sizeof(int);
        break;
      case DW_CFA_advance_loc1:
        {
        unsigned char delta = (unsigned char) *(bytes + 1);
        result[*numInstrs].arg1=delta*codeAlign;
        bytes+=1;
        len -= 1;
        }
      case DW_CFA_advance_loc2:
        {
        unsigned short delta = (unsigned short) *(bytes + 1);
        result[*numInstrs].arg1=delta*codeAlign;
        bytes+=2;
        len -= 2;
        }
        break;
      case DW_CFA_register:
        result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg2Reg=readRegFromLEB128(bytes + 1,&uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_KATANA_fixups:
      case DW_CFA_KATANA_fixups_pointer:
        logprintf(ELL_INFO_V3,ELS_DWARF_FRAME,"Reading DW_CFA_KATANA_fixups or DW_CFA_KATANA_fixups_pointer\n");
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          death("register of unexpected format for po\n");
        }
        bytes+=uleblen;
        len-=uleblen;
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg2Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          death("register of unexpected format for po\n");
        }
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg3=leb128ToUInt(bytes+1,&uleblen);
        logprintf(ELL_INFO_V4,ELS_DWARF_FRAME,"len was %i\n",uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa:
        result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg2=leb128ToUInt(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa_register:
        result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa_offset:
        result[*numInstrs].arg1=leb128ToUInt(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_nop:
        (*numInstrs)--;//since not actually using up an instruction here
        break;
      default:
        death("dwarf cfa instruction 0x%x not yet handled in parseFDEInstructions\n",(uint)low);
      }
    }
  }
  //realloc to free mem we didn't actually use
  result=realloc(result,sizeof(RegInstruction)*(*numInstrs));
  return result;
}

//helper function for readDebugFrame,
//an fde comparison function that can be passed to qsort
int fdeCmp(const void* a,const void* b)
{
  const FDE* fdeA=a;
  const FDE* fdeB=b;
  return fdeA->lowpc-fdeB->lowpc;
}

//returns a Map between the numerical offset of an FDE
//(accessible via the DW_AT_MIPS_fde attribute of the relevant type)
//and the FDE structure
Map* readDebugFrame(ElfInfo* elf)
{
  Dwarf_Fde *fdeData = NULL;
  Dwarf_Signed fdeElementCount = 0;
  Dwarf_Cie *cieData = NULL;
  Dwarf_Signed cieElementCount = 0;
  elf->cie=zmalloc(sizeof(CIE));
  
  Map* result=integerMapCreate(100);//todo: remove arbitrary constant 100
  Dwarf_Error err;
  Dwarf_Debug dbg;
  if(DW_DLV_OK!=dwarf_elf_init(elf->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  if(DW_DLV_OK!=dwarf_get_fde_list(dbg, &cieData, &cieElementCount,
                     &fdeData, &fdeElementCount, &err))
  {
    dwarfErrorHandler(err,NULL);
  }

  //read the CIE
  Dwarf_Unsigned cieLength = 0;
  Dwarf_Small version = 0;
  char* augmenter = "";
  Dwarf_Ptr initInstr = 0;
  Dwarf_Unsigned initInstrLen = 0;
  assert(1==cieElementCount);
  //todo: all the casting here is hackish
  //should respect types more
  if(DW_DLV_OK!=dwarf_get_cie_info(cieData[0],
                     &cieLength,
                     &version,
                     &augmenter,
                     &elf->cie->codeAlign,
                     &elf->cie->dataAlign,
                     &elf->cie->returnAddrRuleNum,
                     &initInstr,
                                   &initInstrLen, &err))
  {
    dwarfErrorHandler(err,NULL);
  }
  //don't care about initial instructions, for patching,
  //but do if we're reading a debug frame for stack unwinding purposes
  //so that we can find activation frames
  
  elf->cie->initialInstructions=parseFDEInstructions(dbg,initInstr,initInstrLen,
                                                     elf->cie->dataAlign,
                                                     elf->cie->codeAlign,
                                                     &elf->cie->numInitialInstructions);
  elf->cie->initialRules=dictCreate(100);//todo: get rid of arbitrary constant 100
  evaluateInstructionsToRules(elf->cie->initialInstructions,elf->cie->numInitialInstructions,
                              elf->cie->initialRules,0,-1);
  
  //todo: bizarre bug, it keeps coming out as -1, which is wrong
  elf->cie->codeAlign=1;
  
  elf->fdes=zmalloc(fdeElementCount*sizeof(FDE));
  elf->numFdes=fdeElementCount;
  for (int i = 0; i < fdeElementCount; i++)
  {
    elf->fdes[i].idx=i;
    Dwarf_Fde dfde=fdeData[i];
    Dwarf_Ptr instrs;
    Dwarf_Unsigned ilen;
    if(DW_DLV_OK!=dwarf_get_fde_instr_bytes(dfde, &instrs, &ilen, &err))
    {
      dwarfErrorHandler(err,NULL);
    }
    elf->fdes[i].instructions=parseFDEInstructions(dbg,instrs,ilen,elf->cie->dataAlign,elf->cie->codeAlign,&elf->fdes[i].numInstructions);
    Dwarf_Addr lowPC = 0;
    Dwarf_Unsigned addrRange = 0;
    Dwarf_Ptr fdeBytes = NULL;
    Dwarf_Unsigned fdeBytesLength = 0;
    Dwarf_Off cieOffset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fdeOffset = 0;
    dwarf_get_fde_range(dfde,
                        &lowPC, &addrRange,
                        &fdeBytes,
                        &fdeBytesLength,
                        &cieOffset, &cie_index,
                        &fdeOffset, &err);
    if(elf->isPO)
    {
      elf->fdes[i].lowpc=lowPC;
      elf->fdes[i].highpc=0;//has no meaning if the fde was read from a patch object
      elf->fdes[i].memSize=addrRange;
    }
    else
    {
      elf->fdes[i].lowpc=lowPC;
      elf->fdes[i].highpc=lowPC+addrRange;
      elf->fdes[i].memSize=0;//has no meaning if the fde wasn't read from a patch object
    }
    elf->fdes[i].offset=fdeOffset;
    int* key=zmalloc(sizeof(int));
    *key=elf->fdes[i].offset;
    mapInsert(result,key,elf->fdes+i);
    dwarf_dealloc(dbg,dfde,DW_DLA_FDE);
  }

  //sort fdes by lowpc unless this is a patch object. This
  //makes determining backtraces easier
  qsort(elf->fdes,elf->numFdes,sizeof(FDE),fdeCmp);
  
  dwarf_dealloc(dbg,cieData[0],DW_DLA_CIE);
  dwarf_dealloc(dbg,fdeData,DW_DLA_LIST);
  dwarf_dealloc(dbg,cieData,DW_DLA_LIST);
  dwarf_finish(dbg,&err);
  return result;
}

