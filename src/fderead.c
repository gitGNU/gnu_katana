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
#include "leb.h"
#include "fderead.h"
#include "register.h"
#include "dwarf_instr.h"
#include <dwarf.h>
#include <assert.h>
#include "util/logging.h"
#include "dwarfvm.h"
#include "elfutil.h"

//create a DwarfExpression object from the raw bytes
DwarfExpr parseDwarfExpression(byte* data,uint len)
{
  DwarfExpr result;
  result.numInstructions=0;
  //allocate more mem than we'll actually need. We can free some
  //later.
  result.instructions=zmalloc(sizeof(DwarfExprInstr)*len);
  for(;len>0;len--,result.numInstructions++)
  {
    DwarfExprInstr* instr=&result.instructions[result.numInstructions];
    instr->type=data[0];
    switch(instr->type)
    {
      //handle all of the operations which take no operands
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
    case DW_OP_reg0:
    case DW_OP_reg1:
    case DW_OP_reg2:
    case DW_OP_reg3:
    case DW_OP_reg4:
    case DW_OP_reg5:
    case DW_OP_reg6:
    case DW_OP_reg7:
    case DW_OP_reg8:
    case DW_OP_reg9:
    case DW_OP_reg10:
    case DW_OP_reg11:
    case DW_OP_reg12:
    case DW_OP_reg13:
    case DW_OP_reg14:
    case DW_OP_reg15:
    case DW_OP_reg16:
    case DW_OP_reg17:
    case DW_OP_reg18:
    case DW_OP_reg19:
    case DW_OP_reg20:
    case DW_OP_reg21:
    case DW_OP_reg22:
    case DW_OP_reg23:
    case DW_OP_reg24:
    case DW_OP_reg25:
    case DW_OP_reg26:
    case DW_OP_reg27:
    case DW_OP_reg28:
    case DW_OP_reg29:
    case DW_OP_reg30:
    case DW_OP_reg31:
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
      //don't need to do anything for these opcodes, they have no
      //operands
      break;
    //handle all the opcodes which take a 1-byte unsigned argument
    case DW_OP_const1u:
    case DW_OP_pick:
    case DW_OP_deref_size:
    case DW_OP_xderef_size:
      memcpy(&instr->arg1,data+1,1);
      data++;
      len--;
      break;
    //handle all the opcodes which take a 1-byte signed argument
    case DW_OP_const1s:
      memcpy(&instr->arg1,data+1,1);
      instr->arg1=sextend(instr->arg1,1);
      data++;
      len--;
      //handle all the opcodes which take a 2-byte unsigned argument
    case DW_OP_const2u:
      memcpy(&instr->arg1,data+1,2);
      data+=2;
      len-=2;
      break;
    //handle all the opcodes which take a 2-byte signed argument
    case DW_OP_const2s:
    case DW_OP_skip:
    case DW_OP_bra:
      memcpy(&instr->arg1,data+1,2);
      instr->arg1=sextend(instr->arg1,2);
      data+=2;
      len-=2;
      break;
      //handle all the opcodes which take a 4-byte argument
    case DW_OP_const4u:
      memcpy(&instr->arg1,data+1,4);
      data+=4;
      len-=4;
      break;
    case DW_OP_const4s:
      memcpy(&instr->arg1,data+1,4);
      instr->arg1=sextend(instr->arg1,4);
      data+=4;
      len-=4;
      break;
      //handle all the opcodes which take an 8-byte arugment
    case DW_OP_const8u:
      //this may be an issue if on 32-bit but presumably these don't
      //get used on 32-bit. We'll add support to katana for it if we
      //need to
      assert(sizeof(instr->arg1>=8));
      memcpy(&instr->arg1,data+1,8);
      data+=8;
      len-=8;
      break;
    case DW_OP_const8s:
      assert(sizeof(instr->arg1>=8));
      memcpy(&instr->arg1,data+1,8);
      instr->arg1=sextend(instr->arg1,8);
      data+=8;
      len-=8;
      break;
    //handle all the opcodes which take a target machine address
    //sized argument
    case DW_OP_addr:
      memcpy(&instr->arg1,data+1,sizeof(addr_t));
      data+=sizeof(addr_t);
      len-=sizeof(addr_t);
      break;
    //handle all the opcodes which take an unsigned LEB argument
    case DW_OP_constu:
    case DW_OP_plus_uconst:
      {
        usint numBytes;
        usint numSeptetsRead;
        byte* number=decodeLEB128(data+1,false,&numBytes,&numSeptetsRead);
        assert(numBytes<=sizeof(instr->arg1));
        memcpy(&instr->arg1,number,numBytes);
        data+=numSeptetsRead;
        len-=numSeptetsRead;
        free(number);
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
        usint numSeptetsRead;
        byte* number=decodeLEB128(data+1,true,&numBytes,&numSeptetsRead);
        assert(numBytes<=sizeof(instr->arg1));
        memcpy(&instr->arg1,number,numBytes);
        data+=numSeptetsRead;
        len-=numSeptetsRead;
        free(number);
      }
      break;
    default:
      death("Unsupported DW_OP with code 0x%x\n",instr->type);
    }
  }

  result.instructions=realloc(result.instructions,sizeof(DwarfExprInstr)*result.numInstructions);
  return result;
}

//the returned memory should be freed
RegInstruction* parseFDEInstructions(Dwarf_Debug dbg,unsigned char* bytes,
                                     int len,int* numInstrs)
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
    case DW_CFA_restore:
      result[*numInstrs].type=high;
      result[*numInstrs].arg1=low;
      break;
    case DW_CFA_offset:
      result[*numInstrs].type=DW_CFA_offset;
      result[*numInstrs].arg1=low;
      result[*numInstrs].arg1Reg.type=ERT_BASIC;
      result[*numInstrs].arg1Reg.u.index=low;
      result[*numInstrs].arg2=leb128ToUWord(bytes + 1, &uleblen);
      bytes+=uleblen;
      len-=uleblen;
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
        result[*numInstrs].arg1=delta;
        bytes+=1;
        len -= 1;
        }
        break;
      case DW_CFA_advance_loc2:
        {
        unsigned short delta = (unsigned short) *(bytes + 1);
        result[*numInstrs].arg1=delta;
        bytes+=2;
        len -= 2;
        }
        break;
      case DW_CFA_advance_loc4:
        {
          uint32 delta = *((uint32*)(bytes + 1));
          result[*numInstrs].arg1=delta;
          bytes+=4;
          len -= 4;
        }
        break;
      case DW_CFA_same_value:
        result[*numInstrs].type=high;
        result[*numInstrs].arg1=low;
        result[*numInstrs].arg1Reg.type=ERT_BASIC;
        result[*numInstrs].arg1Reg.u.index=low;
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
      case DW_CFA_remember_state:
      case DW_CFA_restore_state:
        //don't need to store any arguments, this instruction doesn't take any
        break;
      case DW_CFA_expression:
      case DW_CFA_val_expression:
        result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        bytes+=uleblen;
        len-=uleblen;
        uint exprBytesLen=leb128ToUInt(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].expr=parseDwarfExpression(bytes+1,exprBytesLen);
        bytes+=exprBytesLen;
        len-=exprBytesLen;
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

//returns a Map between the numerical offset of an FDE (accessible via
//the DW_AT_MIPS_fde attribute of the relevant type) and the FDE
//structure.  if ehInsteadOfDebug is true, then read information from
//.eh_frame instead of .debug_frame.
Map* readDebugFrame(ElfInfo* elf,bool ehInsteadOfDebug)
{
  Dwarf_Fde *fdeData = NULL;
  Dwarf_Signed fdeElementCount = 0;
  Dwarf_Cie *cieData = NULL;
  Dwarf_Signed cieElementCount = 0;
  
  Map* result=integerMapCreate(100);//todo: remove arbitrary constant 100
  Dwarf_Error err;
  Dwarf_Debug dbg;
  addr_t* lsdaPointers=NULL;
  if(DW_DLV_OK!=dwarf_elf_init(elf->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }

  Elf_Scn* scn=NULL;
  if(!ehInsteadOfDebug)
  {
    if(DW_DLV_OK!=dwarf_get_fde_list(dbg, &cieData, &cieElementCount,
                                     &fdeData, &fdeElementCount, &err))
    {
      dwarfErrorHandler(err,NULL);
    }
    scn=getSectionByName(elf,".debug_frame");
  }
  else
  {
    elf->callFrameInfo.isEHFrame=true;
    if(DW_DLV_OK!=dwarf_get_fde_list_eh(dbg, &cieData, &cieElementCount,
                                     &fdeData, &fdeElementCount, &err))
    {
      dwarfErrorHandler(err,NULL);
    }
    scn=getSectionByName(elf,".eh_frame");
    Elf_Scn* hdrScn=getSectionByName(elf,".eh_frame_hdr");
    GElf_Shdr shdr;
    getShdr(hdrScn,&shdr);
    elf->callFrameInfo.ehHdrAddress=shdr.sh_addr;
    Elf_Scn* exceptTableSection=getSectionByName(elf,".gcc_except_table");
    if(exceptTableSection)
    {
      getShdr(exceptTableSection,&shdr);
      elf->callFrameInfo.exceptTableAddress=shdr.sh_addr;
      ExceptTable et=parseExceptFrame(exceptTableSection,&lsdaPointers);
      elf->callFrameInfo.exceptTable=zmalloc(sizeof(ExceptTable));
      memcpy(elf->callFrameInfo.exceptTable,&et,sizeof(et));
    }
  }
  GElf_Shdr shdr;
  getShdr(scn,&shdr);
  elf->callFrameInfo.sectionAddress=shdr.sh_addr;
    

  //read the CIE
  Dwarf_Unsigned cieLength = 0;
  Dwarf_Ptr initInstr = 0;
  Dwarf_Unsigned initInstrLen = 0;

  elf->callFrameInfo.cies=zmalloc(sizeof(CIE)*cieElementCount);
  elf->callFrameInfo.numCIEs=cieElementCount;

  for(int i=0;i<cieElementCount;i++)
  {
    char* augmenter = "";
    elf->callFrameInfo.cies[i].idx=i;
    CIE* cie=&elf->callFrameInfo.cies[i];
    //todo: all the casting here is hackish
    //should respect types more
    if(DW_DLV_OK!=dwarf_get_cie_info(cieData[i],
                                     &cieLength,
                                     &cie->version,
                                     &augmenter,
                                     &cie->codeAlign,
                                     &cie->dataAlign,
                                     &cie->returnAddrRuleNum,
                                     &initInstr,
                                     &initInstrLen, &err))
    {
      dwarfErrorHandler(err,NULL);
    }
    //get the augmentation data
    Dwarf_Small* augdata;
    Dwarf_Unsigned augdataLen;
    dwarf_get_cie_augmentation_data(cieData[i],
                                    &augdata,
                                    &augdataLen,
                                    &err);
    //computing the precise address of the augmentation data (for
    //doing pc-relative pointer encodings) is a bit of a pain.
    Dwarf_Off cieOff;
    dwarf_cie_section_offset(dbg,cieData[i],&cieOff,&err);
    addr_t cieAddress=elf->callFrameInfo.sectionAddress+cieOff;
    //ok, now we have the address of the cie. We want the offset to
    //the augmentation todo: the below sizes are all for 32-bit DWARF
    //format, not 64-bit.  (32-bit DWARF format is still most
    //prevalent on 64-bit systems and is not directly correlated to
    //the ELF class)
    addr_t augdataAddress=cieAddress + 4 //the length fields
      + 4 //CIE_id
      + 1 //version byte
      + strlen(augmenter)+1
      + 1 //byte for code align, assuming less than 64
      + 1 //byte for data align, assuming less than 64
      + 1; //byte for returnAddrRegLen assuming the return address
           //register is less than 128
    assert(cie->codeAlign < 64);
    assert(cie->dataAlign < 64);
    assert(cie->returnAddrRuleNum < 64);

    
    parseAugmentationStringAndData(cie,augmenter,augdata,augdataLen,augdataAddress);
    
    //don't care about initial instructions, for patching,
    //but do if we're reading a debug frame for stack unwinding purposes
    //so that we can find activation frames
    elf->callFrameInfo.cies[i].initialInstructions=
      parseFDEInstructions(dbg,initInstr,initInstrLen,
                           &elf->callFrameInfo.cies[i].numInitialInstructions);
    elf->callFrameInfo.cies[i].initialRules=dictCreate(100);//todo: get rid of
    //arbitrary constant 100
    evaluateInstructionsToRules(&elf->callFrameInfo.cies[i],
                                elf->callFrameInfo.cies[i].initialInstructions,
                                elf->callFrameInfo.cies[i].numInitialInstructions,
                                elf->callFrameInfo.cies[i].initialRules,0,-1,NULL);
  
    //todo: bizarre bug, it keeps coming out as -1, which is wrong
    elf->callFrameInfo.cies[i].codeAlign=1;
  }
  
  elf->callFrameInfo.fdes=zmalloc(fdeElementCount*sizeof(FDE));
  elf->callFrameInfo.numFDEs=fdeElementCount;
  for (int i = 0; i < fdeElementCount; i++)
  {
    elf->callFrameInfo.fdes[i].idx=i;
    Dwarf_Fde dfde=fdeData[i];
    Dwarf_Ptr instrs;
    Dwarf_Unsigned ilen;
    if(DW_DLV_OK!=dwarf_get_fde_instr_bytes(dfde, &instrs, &ilen, &err))
    {
      dwarfErrorHandler(err,NULL);
    }

    Dwarf_Cie dcie;
    dwarf_get_cie_of_fde(dfde,&dcie,&err);
    Dwarf_Signed cieIndex;
    dwarf_get_cie_index(dcie,&cieIndex,&err);
    elf->callFrameInfo.fdes[i].cie=&elf->callFrameInfo.cies[cieIndex];
    logprintf(ELL_INFO_V2,ELS_DWARF_FRAME,"Reading instructions in FDE #%i\n",i);
    elf->callFrameInfo.fdes[i].instructions=parseFDEInstructions(dbg,instrs,ilen,&elf->callFrameInfo.fdes[i].numInstructions);
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
      elf->callFrameInfo.fdes[i].lowpc=lowPC;
      elf->callFrameInfo.fdes[i].highpc=0;//has no meaning if the fde was read from a patch object
      elf->callFrameInfo.fdes[i].memSize=addrRange;
    }
    else
    {
      elf->callFrameInfo.fdes[i].lowpc=lowPC;
      elf->callFrameInfo.fdes[i].highpc=lowPC+addrRange;
      elf->callFrameInfo.fdes[i].memSize=0;//has no meaning if the fde wasn't read from a patch object
    }
    elf->callFrameInfo.fdes[i].offset=fdeOffset;

    Dwarf_Small* augdata;
    Dwarf_Unsigned augdataLen;
    dwarf_get_fde_augmentation_data(dfde,
                                    &augdata,
                                    &augdataLen,
                                    &err);

    if(elf->callFrameInfo.isEHFrame && augdataLen)
    {
      //find the address of the augmentation data, because we'll need it
      addr_t augDataOffset=0;
      int iterationOffset=0;
      while(true)
      {
        byte* ptr=memchr(fdeBytes+iterationOffset,augdata[0],fdeBytesLength);
        if(!ptr)
        {
          death("Augmentation data seemingly does not exist when scanning raw data!\n");
        }
        bool isMatch=true;
        for(int j=1;j<augdataLen;j++)
        {
          if(augdata[j]!=ptr[j])
          {
            isMatch=false;
            break;
          }
        }
        if(isMatch)
        {
          augDataOffset=fdeOffset+ptr-(byte*)fdeBytes;
          break;
        }
        else
        {
          iterationOffset=ptr-(byte*)fdeBytes+1;
          continue;
        }
      }
      //turn the offset into an address
      addr_t augDataAddr=elf->callFrameInfo.sectionAddress+augDataOffset;
      parseFDEAugmentationData(elf->callFrameInfo.fdes+i,augDataAddr,
                               augdata,augdataLen,lsdaPointers,elf->callFrameInfo.exceptTable->numLSDAs);
    }
    
    int* key=zmalloc(sizeof(int));
    *key=elf->callFrameInfo.fdes[i].offset;
    mapInsert(result,key,elf->callFrameInfo.fdes+i);
    dwarf_dealloc(dbg,dfde,DW_DLA_FDE);
  }

  //sort fdes by lowpc unless this is a patch object. This
  //makes determining backtraces easier
  qsort(elf->callFrameInfo.fdes,elf->callFrameInfo.numFDEs,sizeof(FDE),fdeCmp);
  
  dwarf_dealloc(dbg,cieData[0],DW_DLA_CIE);
  dwarf_dealloc(dbg,fdeData,DW_DLA_LIST);
  dwarf_dealloc(dbg,cieData,DW_DLA_LIST);
  dwarf_finish(dbg,&err);
  return result;
}

