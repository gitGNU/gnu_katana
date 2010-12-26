/*
  File: leb.c
  Author: James Oakley
  Project:  katana
  Date: October 2010
  Description: utility functions for dealing with LEB numbers
*/

#include "types.h"
#include "leb.h"
#include <math.h>
#include "util/logging.h"

//encode bytes (presumably representing a number)
//as LEB128. The returned pointer should
//be freed when the user is finished with it
//todo: clean this function up. It is not well-written
byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,usint* numBytesOut)
{
  byte* result=encodeAsLEB128NoOptimization(bytes,numBytes,signed_,numBytesOut);
  if(!signed_)
  {
    //clear out zero bytes we don't need
    while((result[(*numBytesOut)-1]&0x7f) == 0)
    {
      (*numBytesOut)-=1;
    }
    result[(*numBytesOut)-1]&=0x7f;//clear the MSB of the new last septet
  }
  //todo: deal with signed as well;
  return result;
}

//like encodeAsLEB128 except doesn't attempt to do any optimization
byte* encodeAsLEB128NoOptimization(byte* bytes,int numBytes,bool signed_,usint* numBytesOut)
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
    for(int j=0;j<bitsRetrieved;j++)
    {
      mask|=1<<(bitOffset+j);
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
      for(int j=0;j<bitsToGet;j++)
      {
        mask|=1<<j;
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
        signExtendBits=(7==signExtendBits)?0:signExtendBits;
        int mask=0;
        for(int j=0;j<signExtendBits;j++)
        {
          mask|=1<<(7-j-1);//-1 because the MSB has a special purpose
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

  #ifdef DEBUG
  logprintf(ELL_INFO_V4,ELS_LEB,"encoded into LEB as follows:\n");
  logprintf(ELL_INFO_V4,ELS_LEB,"bytes : {");
  for(int i=0;i<numBytes;i++)
  {
    logprintf(ELL_INFO_V4,ELS_LEB,"%i%s ",(int)bytes[i],i+1<numBytes?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_LEB,"}\n become: {");
  for(int i=0;i<numSeptets;i++)
  {
    logprintf(ELL_INFO_V4,ELS_LEB,"%i(%i)%s ",(int)result[i],(int)result[i]&0x7F,i+1<numSeptets?",":"");
  }
  logprintf(ELL_INFO_V4,ELS_LEB,"}\n");
  #endif
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


byte* uintToLEB128(usint value,usint* numBytesOut)
{
  int bytesNeeded=1;
  if(value & 0xFF000000)
  {
    //we need all four bytes
    bytesNeeded=4;
  }
  else if(value & 0xFFFF0000)
  {
    bytesNeeded=3;
  }
  else if(value & 0xFFFFFF00)
  {
    bytesNeeded=2;
  }
  return encodeAsLEB128((byte*)&value,bytesNeeded,false,numBytesOut);
}

byte* intToLEB128(int value,usint* numBytesOut)
{
  byte lower7Bits=0x7f;
  byte signExtensionBits=value<0?0x7f:0x00;
  byte signExtension6thBit=value<0?0x40:0x00;
  byte* result=encodeAsLEB128((byte*)&value,4,true,numBytesOut);
  byte sixthBitMask=0x40;//0b01000000
  //now we need to chop off all the bytes we don't need
  for(int i=(*numBytesOut)-1;i>0;i--)
  {
    if((result[i]&lower7Bits)==signExtensionBits &&
       (result[i-1]&sixthBitMask)==signExtension6thBit)
    {
      //this byte is all sign extension bits and there's still one
      //sign extension bit left in the last bit
      (*numBytesOut)--;
    }
    else
    {
      break;
    }
  }
  //since we may have removed bytes make sure the last one has its
  //continuation bit cleared properly
  result[(*numBytesOut)-1]&=lower7Bits;
  return result;
}

uint leb128ToUInt(byte* bytes,usint* outLEBBytesRead)
{
  usint resultBytes;
  //valgrind gives this as a mem leak, but I can't figure out why,
  //as I free the result below. . .
  byte* result=decodeLEB128(bytes,false,&resultBytes,outLEBBytesRead);
  //printf("result bytes is %i\n",resultBytes);
  assert(resultBytes <= sizeof(uint));
  uint val=0;
  memcpy(&val,result,resultBytes);
  free(result);
  return val;
}

word_t leb128ToUWord(byte* bytes,usint* outLEBBytesRead)
{

  usint resultBytes;
  //valgrind gives this as a mem leak, but I can't figure out why,
  //as I free the result below. . .
  byte* result=decodeLEB128(bytes,false,&resultBytes,outLEBBytesRead);
  //printf("result bytes is %i\n",resultBytes);
  assert(resultBytes <= sizeof(word_t));
  word_t val=0;
  memcpy(&val,result,resultBytes);
  free(result);
  return val;
}
