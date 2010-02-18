/*
  File: fderead.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Read the FDE's from the section formatted as a .debug_frame section.
               These FDEs contain instructions used for patching the structure  
*/

#include "dwarftypes.h"
#include "fderead.h"
#include "register.h"
#include "dwarf_instr.h"
#include <libdwarf.h>
#include <dwarf.h>
#include <assert.h>
#include "util/logging.h"



Dwarf_Fde *fdeData = NULL;
Dwarf_Signed fdeElementCount = 0;
Dwarf_Cie *cieData = NULL;
Dwarf_Signed cieElementCount = 0;
CIE cie;


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
        printf("Reading CW_CFA_register\n");
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=leb128ToUInt(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        printf("byte is %i,%i,%i\n",(int)bytes[1],(int)bytes[1],(int)bytes[3]);
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg2Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg2=leb128ToUInt(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_KATANA_do_fixups:
        logprintf(ELL_INFO_V2,ELS_DWARF_FRAME,"Reading CW_CFA_KATANA_do_fixups\n");
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
        //todo: is this 64-bit safe?
        result[*numInstrs].arg3=leb128ToUInt(bytes+1,&uleblen);
        logprintf(ELL_INFO_V4,ELS_DWARF_FRAME,"len was %i\n",uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa:
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=leb128ToUInt(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg2=leb128ToUInt(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa_register:
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=leb128ToUInt(bytes + 1, &uleblen);
        }
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


//returns a Map between the numerical offset of an FDE
//(accessible via the DW_AT_MIPS_fde attribute of the relevant type)
//and the FDE structure
Map* readDebugFrame(ElfInfo* elf)
{
  Map* result=integerMapCreate(100);//todo: remove arbitrary constant 100
  Dwarf_Error err;
  Dwarf_Debug dbg;
  if(DW_DLV_OK!=dwarf_elf_init(elf->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  dwarf_get_fde_list(dbg, &cieData, &cieElementCount,
                                   &fdeData, &fdeElementCount, &err);

  
  //read the CIE
  Dwarf_Unsigned cieLength = 0;
  Dwarf_Small version = 0;
  char* augmenter = "";
  Dwarf_Ptr initInstr = 0;
  Dwarf_Unsigned initInstrLen = 0;
  assert(1==cieElementCount);
  //todo: all the casting here is hackish
  //should respect types more
  dwarf_get_cie_info(cieData[0],
                     &cieLength,
                     &version,
                     &augmenter,
                     &cie.codeAlign,
                     &cie.dataAlign,
                     &cie.returnAddrRuleNum,
                     &initInstr,
                     &initInstrLen, &err);
  //don't care about initial instructions, we don't use them
  /*
  cie.initialInstructions=parseFDEInstructions(dbg,initInstr,initInstrLen,cie.dataAlign,
                                                cie.codeAlign,&cie.numInitialInstructions);
  cie.initialRules=dictCreate(100);//todo: get rid of arbitrary constant 100
  evaluateInstructions(cie.initialInstructions,cie.numInitialInstructions,cie.initialRules,-1,&cie);
  */
  //todo: bizarre bug, it keeps coming out as -1, which is wrong
  cie.codeAlign=1;
  
  elf->fdes=zmalloc(fdeElementCount*sizeof(FDE));
  elf->numFdes=fdeElementCount;
  for (int i = 0; i < fdeElementCount; i++)
  {
    Dwarf_Fde dfde=fdeData[i];
    elf->fdes[i].dfde=dfde;
    Dwarf_Ptr instrs;
    Dwarf_Unsigned ilen;
    dwarf_get_fde_instr_bytes(dfde, &instrs, &ilen, &err);
    elf->fdes[i].instructions=parseFDEInstructions(dbg,instrs,ilen,cie.dataAlign,cie.codeAlign,&elf->fdes[i].numInstructions);
    Dwarf_Addr lowPC = 0;
    Dwarf_Unsigned funcLength = 0;
    Dwarf_Ptr fdeBytes = NULL;
    Dwarf_Unsigned fdeBytesLength = 0;
    Dwarf_Off cieOffset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fdeOffset = 0;
    dwarf_get_fde_range(dfde,
                        &lowPC, &funcLength,
                        &fdeBytes,
                        &fdeBytesLength,
                        &cieOffset, &cie_index,
                        &fdeOffset, &err);
    elf->fdes[i].lowpc=lowPC;
    elf->fdes[i].highpc=lowPC+funcLength;
    elf->fdes[i].offset=fdeOffset;
    int* key=zmalloc(sizeof(int));
    *key=elf->fdes[i].offset;
    mapInsert(result,key,elf->fdes+i);
  }
  return result;
}

