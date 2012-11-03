/*
  File: leb.c
  Author: James Oakley
  Copyright (C): 2010-2011 Dartmouth College
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
  Date: April, 2011
  Description: utility methods for working with LEB numbers
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
  usint numBytesOutInternal;
  byte* result=encodeAsLEB128NoOptimization(bytes,numBytes,signed_,&numBytesOutInternal);
  if((!signed_))
  {
    //clear out zero bytes we don't need
    while((numBytesOutInternal)>1 &&
          ((result[(numBytesOutInternal)-1]&0x7f) == 0))
    {
      numBytesOutInternal-=1;
    }
    result[numBytesOutInternal-1]&=0x7f;//clear the MSB of the new last septet
  }
  else
  {
    byte lower7Bits=0x7f;
    byte signExtensionBits=0x7f;
    byte signExtension6thBit=0x40;
    if(!(bytes[numBytes-1] & 0x80))
    {
      //number is positive, not negative
      signExtension6thBit=0;
      signExtensionBits=0;
    }
    byte sixthBitMask=0x40;//0b01000000
    //now we need to chop off all the bytes we don't need
    for(int i=numBytesOutInternal-1;i>0;i--)
    {
      if((result[i]&lower7Bits)==signExtensionBits &&
         (result[i-1]&sixthBitMask)==signExtension6thBit)
      {
        //this byte is all sign extension bits and there's still one
        //sign extension bit left in the last bit
        numBytesOutInternal--;
      }
      else
      {
        break;
      }
    }
    //since we may have removed bytes make sure the last one has its
    //continuation bit cleared properly
    result[numBytesOutInternal-1]&=lower7Bits;
    
  }
  if(numBytesOut)
  {
    *numBytesOut=numBytesOutInternal;
  }
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
      if(signed_ && bytes[numBytes-1] & 0x80)
      {
        //number is signed and negative, so extend with 1's
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

  if(numBytesOut)
  {
    *numBytesOut=numSeptets;
  }

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

//the returned memory should be freed
byte* decodeLEB128(byte* bytes,bool signed_,usint* numBytesOut,usint* numSeptetsRead)
{
  //do a first pass to determine the number of septets
  int numSeptets=0;
  while(bytes[numSeptets++]&(1<<7))
  {}
  
  //calculate the most possible number of bytes in the result so that
  //we can allocate memory
  int numBytesMax=(int)ceil(((float)numSeptets)*7.0/8.0);
  byte* result=zmalloc(numBytesMax);

  //track the index of the byte in the result array we're currently
  //filling
  int byteIdx=0;

  //track the number of bits left to fill in the current result byte
  int bitsLeftInByte=8;
  for(int i=0;i<numSeptets;i++)
  {
    byte septetBits = bytes[i] & 0x7F;
    int shift = 8-bitsLeftInByte;
    byte val = septetBits<<shift;
    result[byteIdx] |= val;
    if(bitsLeftInByte == 8)
    {
      //then there's still one more bit in this byte
      bitsLeftInByte=1;
    }
    else if(bitsLeftInByte==7)
    {
      //then we exactly filled the byte
      byteIdx++;
      bitsLeftInByte = 8;
    }
    else
    {
      //we filled up the byte but there's still more to write
      byteIdx++;
      //shift the other way to get the bits that were at the top of the septet
      //8-shift is same as bitsLeftInByte, but this is clearer
      val = septetBits>>(8-shift);
      result[byteIdx]=val;
      bitsLeftInByte++; //we use up 7 bits total and get 8 more from
                        //the new byte, so +1
    }
  }
  if(signed_)
  {
    //we might have to sign-extend. By default we're extended with
    //0's. See if we need to extend with 1's.
    if(bitsLeftInByte < 8 &&
       result[byteIdx] & (1<<(7-bitsLeftInByte)))
    {
      for(int i=8-bitsLeftInByte;i<=7;i++)
      {
        result[byteIdx] |= (1 << i);
      }
    }
  }
  else
  {
    if(bitsLeftInByte!=8 && result[byteIdx]==0 && byteIdx>0)
    {
      //unsigned so a 0 byte is meaningless.  Decrease byteIdx to our
      //numBytes calculation will be correct note this makes
      //bitsLeftInByte invalid, but we don't care about it any more
      byteIdx--;
    }
  }
  //now calculate the actual number of bytes
  int numBytes = bitsLeftInByte==8 ? byteIdx : byteIdx+1;
  if(numBytes < numBytesMax)
  {
    result=realloc(result,numBytes);
    MALLOC_CHECK(result);
  }
  if(numSeptetsRead)
  {
    *numSeptetsRead=numSeptets;
  }
  if(numBytesOut)
  {
    *numBytesOut=numBytes;
  }
  return result;
}

byte* uintToLEB128(uint value,usint* numBytesOut)
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
  if(value >= 0)
  {
    return uintToLEB128(value,numBytesOut);
  }
  
  usint numBytes;
  byte* result=encodeAsLEB128((byte*)&value,sizeof(int),true,&numBytes);
  if(numBytesOut)
  {
    *numBytesOut=numBytes;
  }
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

int leb128ToInt(byte* bytes,usint* outLEBBytesRead)
{
  usint resultBytes;
  //valgrind gives this as a mem leak, but I can't figure out why,
  //as I free the result below. . .
  byte* result=decodeLEB128(bytes,true,&resultBytes,outLEBBytesRead);
  //printf("result bytes is %i\n",resultBytes);
  assert(resultBytes <= sizeof(int));
  int val=0;
  memcpy(&val,result,resultBytes);
  if(resultBytes < sizeof(val))
  {
    val=sextend(val,resultBytes);
  }
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

sword_t leb128ToSWord(byte* bytes,usint* outLEBBytesRead)
{
  usint resultBytes;
  //valgrind gives this as a mem leak, but I can't figure out why,
  //as I free the result below. . .
  byte* result=decodeLEB128(bytes,true,&resultBytes,outLEBBytesRead);
  assert(resultBytes <= sizeof(word_t));
  sword_t val=0;
  memcpy(&val,result,resultBytes);
  if(resultBytes < sizeof(val))
  {
    val=sextend(val,resultBytes);
  }
  free(result);
  return val;
}
