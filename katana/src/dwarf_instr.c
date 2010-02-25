/*
  File: dwarf_instr.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Functions for manipulating dwarf instructions
*/

#include "util/util.h"
#include "dwarf_instr.h"
#include <math.h>
#include <dwarf.h>
#include "util/logging.h"

//encode bytes (presumably representing a number)
//as LEB128. The returned pointer should
//be freed when the user is finished with it
byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,usint* numBytesOut)
{
  int numSeptets=ceil((float)numBytes*8.0/7.0);
  byte* result=zmalloc(numSeptets);
  int byteOffset=0;
  int bitOffset=0;//offset into the current byte
  for(int i=0;i<numSeptets;i++)
  {
    //logprintf(ELL_INFO_V4,ELS_MISC,"byte offset is %i and bitOffset is %i\n",byteOffset,bitOffset);
    //shift down to the bits we actually want, then shift up to
    //the position we want them in
    int bitsRetrieved=min(7,8-bitOffset);
    int shift=bitOffset;
    int mask=0;
    for(int i=0;i<bitsRetrieved;i++)
    {
      mask|=1<<(bitOffset+i);
    }
    //logprintf(ELL_INFO_V4,ELS_MISC,"mask is %i and shift is %i\n",mask,shift);
    byte val=(mask&bytes[byteOffset])>>shift;
    //logprintf(ELL_INFO_V4,ELS_MISC,"bits retrieved is %i and that value is %i\n",bitsRetrieved,(int)val);
    if(bitsRetrieved<7 && byteOffset+1<numBytes) //didn't get a full 7 bits before
                                                 //and have room to get more
    {
      int bitsToGet=7-bitsRetrieved;
      int mask=0;
      //we always get bits first from the LSB of a byte
      for(int i=0;i<bitsToGet;i++)
      {
        mask|=1<<i;
      }
      //logprintf(ELL_INFO_V4,ELS_MISC,"getting %i more bits. previously val was %i\n",bitsToGet,(int)val);
      //logprintf(ELL_INFO_V4,ELS_MISC,"next byte is %i, masking it with %i\n",(int)bytes[byteOffset+1],mask);
      //here we're putting them in the MSB of the septet
      val|=(mask&bytes[byteOffset+1])<<(bitsRetrieved);
    }
    bitOffset+=7;
    if(bitOffset>=8)
    {
      bitOffset-=8;
      byteOffset++;
    }
    if(i+1<numSeptets)
    {
      val|=1<<7;//more bytes to come
    }
    else
    {
      if(signed_)
      {
        int signExtendBits=7-(numBytes*8)%7;
        signExtendBits=7==signExtendBits?0:signExtendBits;
        int mask=0;
        for(int j=0;j<signExtendBits;j++)
        {
          mask|=1<<(7-j);
        }
        if(val&1<<6)
        {
          //negative number
          val&= ~mask;
        }
        else
        {
          //positive number
          val|=mask;
        }
      }
      val&=0x7F;//end of the LEB
    }
    result[i]=val;
  }
  *numBytesOut=numSeptets;
  
  /*  logprintf(ELL_INFO_V4,ELS_MISC,"encoded into LEB as follows:\n");
  logprintf(ELL_INFO_V4,ELS_MISC,"bytes : {");
  for(int i=0;i<numBytes;i++)
  {
    logprintf(ELL_INFO_V4,ELS_MISC,"%i%s ",(int)bytes[i],i+1<numBytes?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"}\n become: {");
  for(int i=0;i<numSeptets;i++)
  {
    logprintf(ELL_INFO_V4,ELS_MISC,"%i(%i)%s ",(int)result[i],(int)result[i]&0x7F,i+1<numSeptets?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"}\n");*/
  return result;
}


byte* decodeLEB128(byte* bytes,bool signed_,usint* numBytesOut,usint* numSeptetsRead)
{
  //do a first pass to determine the number of septets
  int numSeptets=0;
  while(bytes[numSeptets++]&(1<<7)){}
  int numBytes=max(1,(int)floor(numSeptets*7.0/8.0));//floor because may have been sign extended. max because otherwise if only one septet this will give 0 bytes
  //todo: not positive the above is correct
  byte* result=zmalloc(numBytes);
  int byteOffset=0;
  int bitOffset=0;//offset into the current byte
  for(int i=0;i<numSeptets;i++)
  {
    //logprintf(ELL_INFO_V4,ELS_MISC,"byte offset is %i and bitOffset is %i\n",byteOffset,bitOffset);
    //if there is a bit offset into the byte, will be filling
    //starting above the LSB
    //construct a mask as appropriate to mask out parts of LEB value we don't want
    int mask=0;
    int bitsRetrieved=min(7,8-bitOffset);
    for(int i=0;i<bitsRetrieved;i++)
    {
      mask|=1<<i;
    }
    byte val=bytes[i]&mask;
    int shift=0==bitOffset?0:8-bitsRetrieved;
    //logprintf(ELL_INFO_V4,ELS_MISC,"mask is %i and val is %i, and shift is %i\n",mask,(int)val,shift);
    result[byteOffset]|=val<<shift;
    //logprintf(ELL_INFO_V4,ELS_MISC,"byte so far is %i\n",(int)result[byteOffset]);
    if(bitsRetrieved<7 && byteOffset+1<numBytes)
    {
      int bitsToGet=7-bitsRetrieved;
      //logprintf(ELL_INFO_V4,ELS_MISC,"need to get %i additional bits from this septet with val %i(%i)\n",bitsToGet,(int)val,(int)bytes[i]&0x7F);
      //need to construct mask so we don't read too much
      byte mask=0;
      for(int j=0;j<bitsToGet;j++)
      {
        mask|=1<<(bitsRetrieved+j);
      }
      //logprintf(ELL_INFO_V4,ELS_MISC,"mask for additional bytes is %u\n",(uint)mask);
      result[byteOffset+1]=(mask&bytes[i])>>bitsRetrieved;
      //logprintf(ELL_INFO_V4,ELS_MISC,"after getting those bits, next byte is %i\n",result[byteOffset+1]);
    }
    bitOffset+=7;
    if(bitOffset>=8)
    {
      bitOffset-=8;
      byteOffset++;
    }
  }
  *numBytesOut=numBytes;
  *numSeptetsRead=numSeptets;
  /*logprintf(ELL_INFO_V4,ELS_MISC,"decoded from LEB as follows:\n");
  /logprintf(ELL_INFO_V4,ELS_MISC,"leb bytes : {");
  for(int i=0;i<numSeptets;i++)
  {
    logprintf(ELL_INFO_V4,ELS_MISC,"%i(%i)%s ",(int)bytes[i],(int)bytes[i]&0x7F,i+1<numSeptets?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"}\n become: {");
  for(int i=0;i<numBytes;i++)
  {
    logprintf(ELL_INFO_V4,ELS_MISC,"%i%s ",(int)result[i],i+1<numBytes?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_MISC,"}\n");*/
  return result;
}

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
  case DW_CFA_offset_extended_sf:
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

uint leb128ToUInt(byte* bytes,usint* outLEBBytesRead)
{
  usint resultBytes;
  byte* result=decodeLEB128(bytes,false,&resultBytes,outLEBBytesRead);
  uint val=0;
  memcpy(&val,result,resultBytes);
  return val;
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
    printf("DW_CFA_offset r%i %i\n",inst.arg1,inst.arg2);
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
      printf("r%i ",inst.arg2);
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
      printf("r%i ",inst.arg2);
    }
    else
    {
      printReg(inst.arg2Reg,stdout);
      printf(" ");
    }
    printf("fde#%i ",inst.arg3);
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
    printf("%i \n",inst.arg2);
    break;
  case DW_CFA_def_cfa_register:
    printf("DW_CFA_def_cfa_register");
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
