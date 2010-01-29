/*
  File: dwarf_instr.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Functions for manipulating dwarf instructions
*/

#include "util.h"
#include "dwarf_instr.h"
#include <math.h>
#include <dwarf.h>

//encode bytes (presumably representing a number)
//as LEB128. The returned pointer should
//be freed when the user is finished with it
byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,short int* numBytesOut)
{
  int numSeptets=ceil((float)numBytes*8.0/7.0);
  byte* result=zmalloc(numSeptets);
  int byteOffset=0;
  int bitOffset=0;//offset into the current byte
  for(int i=0;i<numSeptets;i++)
  {
    //printf("byte offset is %i and bitOffset is %i\n",byteOffset,bitOffset);
    //shift down to the bits we actually want, then shift up to
    //the position we want them in
    int bitsRetrieved=min(7,8-bitOffset);
    int shift=bitOffset;
    int mask=0;
    for(int i=0;i<bitsRetrieved;i++)
    {
      mask|=1<<(bitOffset+i);
    }
    //printf("mask is %i and shift is %i\n",mask,shift);
    byte val=(mask&bytes[byteOffset])>>shift;
    //printf("bits retrieved is %i and that value is %i\n",bitsRetrieved,(int)val);
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
      //printf("getting %i more bits. previously val was %i\n",bitsToGet,(int)val);
      //printf("next byte is %i, masking it with %i\n",(int)bytes[byteOffset+1],mask);
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
  
  printf("encoded into LEB as follows:\n");
  printf("bytes : {");
  for(int i=0;i<numBytes;i++)
  {
    printf("%i%s ",(int)bytes[i],i+1<numBytes?",":"");
  }
  printf("}\n become: {");
  for(int i=0;i<numSeptets;i++)
  {
    printf("%i(%i)%s ",(int)result[i],(int)result[i]&0x7F,i+1<numSeptets?",":"");
  }
  printf("}\n");
  return result;
}


byte* decodeLEB128(byte* bytes,bool signed_,short int* numBytesOut,short int* numSeptetsRead)
{
  //do a first pass to determine the number of septets
  int numSeptets=0;
  while(bytes[numSeptets++]&(1<<7)){}
  int numBytes=(int)floor(numSeptets*7.0/8.0);//floor because may have been sign extended
  byte* result=zmalloc(numBytes);
  int byteOffset=0;
  int bitOffset=0;//offset into the current byte
  for(int i=0;i<numSeptets;i++)
  {
    //printf("byte offset is %i and bitOffset is %i\n",byteOffset,bitOffset);
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
    //printf("mask is %i and val is %i, and shift is %i\n",mask,(int)val,shift);
    result[byteOffset]|=val<<shift;
    //printf("byte so far is %i\n",(int)result[byteOffset]);
    if(bitsRetrieved<7 && byteOffset+1<numBytes)
    {
      int bitsToGet=7-bitsRetrieved;
      //printf("need to get %i additional bits from this septet with val %i(%i)\n",bitsToGet,(int)val,(int)bytes[i]&0x7F);
      //need to construct mask so we don't read too much
      byte mask=0;
      for(int j=0;j<bitsToGet;j++)
      {
        mask|=1<<(bitsRetrieved+j);
      }
      //printf("mask for additional bytes is %u\n",(uint)mask);
      result[byteOffset+1]=(mask&bytes[i])>>bitsRetrieved;
      //printf("after getting those bits, next byte is %i\n",result[byteOffset+1]);
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
  /*printf("decoded from LEB as follows:\n");
  /printf("leb bytes : {");
  for(int i=0;i<numSeptets;i++)
  {
    printf("%i(%i)%s ",(int)bytes[i],(int)bytes[i]&0x7F,i+1<numSeptets?",":"");
  }
  printf("}\n become: {");
  for(int i=0;i<numBytes;i++)
  {
    printf("%i%s ",(int)result[i],i+1<numBytes?",":"");
  }
  printf("}\n");*/
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
    {
      char buf[32];
      snprintf(buf,32,"Dwarf instruction with opcode %i not supported\n",instr->opcode);
      death(buf);
    }
    break;
  }
}
