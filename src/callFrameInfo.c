/*
  File: callFrameInfo.c
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
  Date: February, 2011
  Description: Data structures and methods dealing with call frame information
*/

#include "callFrameInfo.h"
#include "constants.h"
#include "leb.h"
#include "util/logging.h"
#include "elfutil.h"
#include "util/growingBuffer.h"
#include "eh_pe.h"

//reads the CIE's augmentationData and tries to set up
//the cie->augmentationInfo
//see the Linux Standards Base at
//http://refspecs.freestandards.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
//for augmentation data information. Also in the x86_64 ABI
void parseAugmentationStringAndData(CIE* cie,char* augmentationString,byte* data,int len,addr_t augDataAddress)
{
  int cnt=strlen(augmentationString);
  int offset=0;
  for(int i=0;i<cnt;i++)
  {
    assert(offset<len);
    char c=augmentationString[i];
    switch(c)
    {
    case 'z':
      cie->augmentationFlags|=CAF_DATA_PRESENT;
      break;
    case 'R':
      cie->augmentationFlags|=CAF_FDE_ENC;
      cie->fdePointerEncoding=data[offset];
      offset++;
      break;
    case 'P':
      {
        cie->personalityPointerEncoding=data[offset];
        offset++;
        usint numBytesRead;
        cie->personalityFunction=decodeEHPointer(data+offset,len-offset,augDataAddress+offset,
                                                 cie->personalityPointerEncoding,
                                                 &numBytesRead);
        offset+=numBytesRead;
        cie->augmentationFlags|=CAF_PERSONALITY;
      }
      break;
    case 'L':
      cie->augmentationFlags|=CAF_FDE_LSDA;
      cie->fdeLSDAPointerEncoding=data[offset];
      offset++;
    default:
      break;
    }
  }
}

void parseFDEAugmentationData(FDE* fde,addr_t augDataAddress,byte* augmentationData,int augmentationDataLen,addr_t* lsdaPointers,int numLSDAPointers)
{
  //todo: are there any other types of augmentation we may encounter?
  if(fde->cie->augmentationFlags & CAF_FDE_LSDA)
  {
    if(augmentationDataLen <
       getPointerSizeFromEHPointerEncoding(fde->cie->fdeLSDAPointerEncoding))
    {
      death("Mismatch between CIE and FDE: FDE does not have expected LSDA pointer augmentation data\n");
    }
    addr_t lsdaPointer=
      decodeEHPointer(augmentationData,
                      augmentationDataLen,
                      augDataAddress,
                      fde->cie->fdeLSDAPointerEncoding,NULL);
    fde->hasLSDAPointer=true;
    for(int i=0;i<numLSDAPointers;i++)
    {
      if(lsdaPointers[i]==lsdaPointer)
      {
        fde->lsdaIdx=i;
        return;
      }
    }
    death("FDE appears to have an invalid LSDA pointer\n");
    
  }
}

void buildCIERawData(CIE* cie,GrowingBuffer* buf,CallFrameInfo* cfi)
{
  //todo: support 64-bit DWARF format. Here I am always building 32-bit dwarf format

  //part one of the header, before the augmentation string
  struct
  {
    uint length;
    uint CIE_id;
    byte version;
  } __attribute__((__packed__)) header1;

  //part2 of the header, after the augmentation string
  //only present in Dwarf v4
  struct
  {
    byte address_size;
    byte segment_size;
  } __attribute__((__packed__)) header2;

  //we'll set the length last as we don't know it yet
  header1.CIE_id=cfi->isEHFrame?EH_CIE_ID:DEBUG_CIE_ID;
  header1.version=cie->version;
  assert(cie->addressSize<256);
  assert(cie->segmentSize<256);
  header2.address_size=cie->addressSize;
  header2.segment_size=cie->segmentSize;

  //compute the LEB values we need: code-align, data-align, and return-address-rule
  usint codeAlignLen,dataAlignLen,returnAddrRegLen;
  byte* codeAlign=uintToLEB128(cie->codeAlign,&codeAlignLen);
  byte* dataAlign=intToLEB128(cie->dataAlign,&dataAlignLen);
  byte* returnAddrReg=uintToLEB128(cie->returnAddrRuleNum,&returnAddrRegLen);

  //used to stare the LEB representation of the length of the augmentation data
  byte* augmentationDataLength;
  usint augmentationDataLengthLen=0;
  //the actual raw augmentation data
  byte cieAugmentationData[32];
  //the length of the above data
  int cieAugmentationDataLen=0;
  char augmentationString[16];
  augmentationString[0]='\0';
  
  //from the Linux Standard's Base
  //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  if(cie->augmentationFlags & CAF_DATA_PRESENT)
  {
    int offset=0;
    augmentationString[0]='z';
    augmentationString[1]='\0';
    //construct the cie augmentation data buffer and augmentation string
    //create the string first
    if(cie->augmentationFlags & CAF_PERSONALITY)
    {
      strcat(augmentationString,"P");
    }
    if(cie->augmentationFlags & CAF_FDE_LSDA)
    {
      strcat(augmentationString,"L");
    }
    if(cie->augmentationFlags & CAF_FDE_ENC)
    {
      strcat(augmentationString,"R");
    }
    //and then the data buffer
    if(cie->augmentationFlags & CAF_PERSONALITY)
    {
      if(sizeof(cie->personalityFunction)<=4 ||
         (cie->personalityFunction & 0xFFFFFFFF00000000) == 0)
      {
        //pointer can fit within 32 bits
        byte encoding=cie->personalityPointerEncoding;
        cieAugmentationData[offset]=encoding;
        offset++;
        int numBytesOut;
        addr_t pointerLocation=cfi->sectionAddress+buf->len+sizeof(header1)+strlen(augmentationString)+1+codeAlignLen+dataAlignLen+returnAddrRegLen+offset;
        addr_t personalityFunction=encodeEHPointerFromEncoding(cie->personalityFunction,encoding,pointerLocation,&numBytesOut);
        memcpy(cieAugmentationData+offset,&personalityFunction,numBytesOut);
        offset+=numBytesOut;
      }
      else
      {
        //need 8 bytes
        cieAugmentationData[offset]=DW_EH_PE_udata8;
        offset++;
        memcpy(cieAugmentationData+offset,&cie->personalityFunction,4);
        offset+=8;
      }
    }
    if(cie->augmentationFlags & CAF_FDE_LSDA)
    {
      cieAugmentationData[offset++]=cie->fdeLSDAPointerEncoding;
    }
    if(cie->augmentationFlags & CAF_FDE_ENC)
    {
      cieAugmentationData[offset++]=cie->fdePointerEncoding;
    }
    cieAugmentationDataLen=offset;
    augmentationDataLength=uintToLEB128(cieAugmentationDataLen,&augmentationDataLengthLen);
  }
  

  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(cie->initialInstructions,
                                  cie->numInitialInstructions);

  header1.length=sizeof(header1)-sizeof(header1.length)+strlen(augmentationString)+1+codeAlignLen+dataAlignLen+returnAddrRegLen+cieAugmentationDataLen+augmentationDataLengthLen+rawInstructions.numBytes;
  if(cie->version >= 4)
  {
    header1.length+=sizeof(header2);
  }
  //CIE structures are required to be aligned
  int paddingNeeded=0;
  if(header1.length % cie->addressSize)
  {
    paddingNeeded=cie->addressSize - (header1.length % cie->addressSize);
    header1.length+=paddingNeeded;
  }

  byte* padding=malloc(paddingNeeded);
  MALLOC_CHECK(padding);
  memset(padding,DW_CFA_nop,paddingNeeded);

  //now we can actually write everythingo out
  addToGrowingBuffer(buf,&header1,sizeof(header1));
  addToGrowingBuffer(buf,augmentationString,strlen(augmentationString)+1);
  if(cie->version >= 4)
  {
    addToGrowingBuffer(buf,&header2,sizeof(header2));
  }
  addToGrowingBuffer(buf,codeAlign,codeAlignLen);
  addToGrowingBuffer(buf,dataAlign,dataAlignLen);
  addToGrowingBuffer(buf,returnAddrReg,returnAddrRegLen);
  if(augmentationDataLengthLen)
  {
    addToGrowingBuffer(buf,augmentationDataLength,augmentationDataLengthLen);
  }
  if(cieAugmentationDataLen)
  {
    addToGrowingBuffer(buf,cieAugmentationData,cieAugmentationDataLen);
  }
  addToGrowingBuffer(buf,rawInstructions.instrs,rawInstructions.numBytes);
  addToGrowingBuffer(buf,padding,paddingNeeded);
  destroyRawInstructions(rawInstructions);
}

void buildFDERawData(CallFrameInfo* cfi,FDE* fde,uint* cieOffsets,addr_t* lsdaPointers,GrowingBuffer* buf)
{
  //todo: support 64-bit DWARF format. Here I am always building 32-bit dwarf format

  //part one of the header, before the augmentation string
  
  //note that we aren't able to deal with DWARF stuff
  //cross-architecture this should work on both 64 and 32 bit machines
  //but the architecture of the machine must match the ELF object being
  //dealt with
  struct
  {
    uint length;
    int CIE_offset;
  } __attribute__((__packed__)) header;

  //would be different for 64-bit DWARF format for .debug_frame
  //for .eh_frame, however, it is always 4
  //http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  assert(sizeof(header.length)==4);



  assert(fde->cie);
  

  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(fde->instructions,
                                  fde->numInstructions);

  if(cfi->isEHFrame)
  {
    //according to LSB 3.0 the CIE_pointer is not actually an offset into .eh_frame,
    //but is fde_offset - cie_offset
    //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
    header.CIE_offset=buf->len-cieOffsets[fde->cie->idx]+sizeof(header.length);
  }
  else
  {
    header.CIE_offset=cieOffsets[fde->cie->idx];
  }
  

  addr_t initialLocation;
  int initialLocationByteLen;
  addr_t addressRange;
  int addressRangeByteLen;

  if(cfi->isEHFrame)
  {
    if(!cfi->sectionAddress)
    {
      logprintf(ELL_WARN,ELS_DWARF_FRAME,"While writing binary .eh_frame, it appears taht sectionAddress is not set. This means that all of the addresses within the FDE will be wrong. Relocations are not yet possible\n");
    }
    addr_t pointerLocation=cfi->sectionAddress+buf->len+sizeof(header);
    initialLocation=
      encodeEHPointerFromEncoding(fde->lowpc,
                                  fde->cie->fdePointerEncoding,
                                  pointerLocation,&initialLocationByteLen);
    logprintf(ELL_INFO_V3,ELS_DWARF_BUILD,"Transforming pointer %p to %p for FDE\n",fde->lowpc,initialLocation);
  }
  else
  {
    initialLocation=fde->lowpc;
    initialLocationByteLen=fde->cie->addressSize;
  }
  addressRange=fde->highpc-fde->lowpc;
  if(cfi->isEHFrame)
  {
    //it appears that in .eh_frame this is alwasy 4 although the LSB
    //documentation is most unclear on this point
    addressRangeByteLen=4;
  }
  else
  {
    addressRangeByteLen=fde->cie->addressSize;
  }
  assert(addressRangeByteLen <= sizeof(addressRange));

  //store a LEB representation of the length
  byte* augmentationDataLength;
  usint augmentationDataLengthLen=0;
  //the actual augmentation data to write out
  byte augmentationData[128];
  int augmentationDataLen=0;
    
  //from the Linux Standard's Base
  //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  if(fde->cie->augmentationFlags & CAF_DATA_PRESENT)
  {
    if(fde->cie->augmentationFlags & CAF_FDE_LSDA)
    {
      if(fde->hasLSDAPointer)
      {
        //+1 is because it will take one byte to encode the
        //augmentation data length. We haven't computed it yet, which
        //is why we don't use +augmenationDataLengthLen
        addr_t augmentationDataAddress=cfi->sectionAddress+buf->len+sizeof(header)+
          initialLocationByteLen+addressRangeByteLen + 1;
        addr_t encodedLSDAPointer=
          encodeEHPointerFromEncoding(lsdaPointers[fde->lsdaIdx],
                                      fde->cie->fdeLSDAPointerEncoding,
                                      augmentationDataAddress,
                                      &augmentationDataLen);
        memcpy(augmentationData,&encodedLSDAPointer,augmentationDataLen);
      }
      else
      {
        logprintf(ELL_ERR,ELS_DWARF_FRAME,"FDE doesn't have an LSDA pointer even though its CIE says it should\n");
      }
    }
    else if(fde->hasLSDAPointer)
    {
      logprintf(ELL_WARN,ELS_DWARF_FRAME,"Ignoring LSDA pointer for FDE referencing a CIE without the appropriate augmentation\n");
    }
    augmentationDataLength=uintToLEB128(augmentationDataLen,&augmentationDataLengthLen);
  }


  //now we finially have enough information to compute the length and padding
  header.length=sizeof(header)-sizeof(header.length)+initialLocationByteLen+
    addressRangeByteLen+augmentationDataLengthLen+augmentationDataLen+
    rawInstructions.numBytes;
  int paddingNeeded=0;
  byte* padding=NULL;
  if(header.length % fde->cie->addressSize)
  {
    paddingNeeded=fde->cie->addressSize - (header.length % fde->cie->addressSize);
    header.length+=paddingNeeded;
    padding=malloc(paddingNeeded);
    MALLOC_CHECK(padding);
    memset(padding,DW_CFA_nop,paddingNeeded);
  }


  //now we can actually write everythingo out
  addToGrowingBuffer(buf,&header,sizeof(header));
  addToGrowingBuffer(buf,&initialLocation,initialLocationByteLen);
  addToGrowingBuffer(buf,&addressRange,addressRangeByteLen);
  if(fde->cie->augmentationFlags & CAF_DATA_PRESENT)
  {
    addToGrowingBuffer(buf,augmentationDataLength,augmentationDataLengthLen);
    if(augmentationDataLen)
    {
      addToGrowingBuffer(buf,augmentationData,augmentationDataLen);
    }
  }
  addToGrowingBuffer(buf,rawInstructions.instrs,rawInstructions.numBytes);
  if(paddingNeeded)
  {
    addToGrowingBuffer(buf,padding,paddingNeeded);
  }
  destroyRawInstructions(rawInstructions);
}

typedef struct
{
  int32 initialLocation;
  int32 fdeAddress;
} EhFrameHdrTableEntry;

//the lsdaPointers parameter is used to store the transformation from an LSDA
//index to the actual address that the LSDA lives at. This will be necessary when writing the FDEs
void buildExceptTableRawData(CallFrameInfo* cfi,GrowingBuffer* buf,
                             addr_t** lsdaPointers)
{
  ExceptTable* table=cfi->exceptTable;
  assert(table);
  *lsdaPointers=zmalloc(table->numLSDAs*sizeof(addr_t));
  for(int i=0;i<table->numLSDAs;i++)
  {
    //update the map
    (*lsdaPointers)[i]=cfi->exceptTableAddress+buf->len;
    
    //now get on with actually building the binary data
    LSDA* lsda=table->lsdas+i;
    addr_t lsdaStartOffset=buf->len;
    //gcc hasn't been writing lpstart (other than DW_PE_omit for the
    //encoding) so I'm guessing at proper behaviour here.
    if(lsda->lpStart!=0)
    {
      byte enc=DW_EH_PE_uleb128;
      addToGrowingBuffer(buf,&enc,1);
      addUlebToGrowingBuffer(buf,lsda->lpStart);
    }
    else
    {
      byte enc=DW_EH_PE_omit;
      addToGrowingBuffer(buf,&enc,1);
    }
    //encoding used by entries in the type table
    byte ttTypeFormat=DW_EH_PE_udata4;
    addToGrowingBuffer(buf,&ttTypeFormat,1);

    //unfortunately we now run into a problem. We need to write out
    //the TType base offset as a uleb128. LEB values vary in length,
    //but we don't yet know what the offset is so we can't tell how
    //long it will be. We solve this with the inefficient solution of
    //creating a second buffer which we will copy into the first
    //later. We also don't know how long the call site table will be

    //in order to properly create parts of the call-site table,
    //however, we need the action table. 
    GrowingBuffer actionBuf;
    ZERO(actionBuf);
    addr_t* actionIdxToOffset=zmalloc(sizeof(addr_t)*lsda->numActionEntries);
    for(int j=0;j<lsda->numActionEntries;j++)
    {
      if(actionIdxToOffset[j])
      {
        //we already wrote this action. Skip it now.
        break;
      }
      int idx=j;
      while(true)
      {
        actionIdxToOffset[idx]=actionBuf.len;
        ActionRecord* action=lsda->actionTable+idx;
        //note we make the index 1-based when writing it out
        addUlebToGrowingBuffer(&actionBuf,action->typeFilterIndex+1);
        if(action->hasNextAction)
        {
          if(actionIdxToOffset[action->nextAction] || action->nextAction==0)
          {
            //the next action is an action we've already written out. Construct a self-relative offset.
            sword_t offset=(sword_t)actionIdxToOffset[action->nextAction] - (sword_t)buf->len;
            addSlebToGrowingBuffer(&actionBuf,offset);
            //don't need to follow the chain any further, it's already been written out.
            break;
          }
          else
          {
            //we'll just choose to write the next action next. In that case the offset is 1 byte.
            byte offset=1;
            addToGrowingBuffer(&actionBuf,&offset,1);
            //the next iteration of this while loop will continue writing this chain
            idx=action->nextAction;
          }
        }
        else
        {
          //just write a zero to show there is no next action
          byte nextAction=0;
          addToGrowingBuffer(&actionBuf,&nextAction,1);
          //and don't need to write out any actions in this chain.
          break;
        }
      }
    }

    //write out the call site table
    GrowingBuffer callSiteBuf;
    ZERO(callSiteBuf);
    for(int j=0;j<lsda->numCallSites;j++)
    {
      CallSiteRecord* callSite=&lsda->callSiteTable[j];
      addUlebToGrowingBuffer(&callSiteBuf,callSite->position);
      addUlebToGrowingBuffer(&callSiteBuf,callSite->length);
      addUlebToGrowingBuffer(&callSiteBuf,callSite->landingPadPosition);
      if(callSite->hasAction)
      {
        if(callSite->firstAction >= lsda->numActionEntries)
        {
          death("Invalid firstAction for call site, index is too high\n");
        }
        //note that we make the offset one more than it really is
        //because of gcc convention to tell whether or not there is an
        //action at all
        addUlebToGrowingBuffer(&callSiteBuf,1+actionIdxToOffset[callSite->firstAction]);
      }
      else
      {
        byte noAction=0;
        addToGrowingBuffer(&callSiteBuf,&noAction,1);
      }
    }

    GrowingBuffer typeTableBuf;
    ZERO(typeTableBuf);
    for(int j=lsda->numTypeEntries-1;j>=0;j--)
    {
      //it appears empirically that this is the format the type table
      //takes. Not really governed by any standard.
      addToGrowingBuffer(&typeTableBuf,lsda->typeTable+j,4);
    }

    //ok, now we have the messy problem of calculating the offset of
    //the end of the type table from the location at which we write
    //the offset of the type table. This is icky because it depends on
    //the alignment of the type table, which must be 4-byte aligned
    //and that alignment depends on how many bytes are taken up by
    //writing the offset. We kludgily solve this by always making sure
    //we have extra space to move things around by a few bytes.


    usint callSiteLengthLEBNumBytes;
    byte* callSiteLengthLEB=encodeAsLEB128((byte*)&callSiteBuf.len,sizeof(callSiteBuf.len),false,&callSiteLengthLEBNumBytes);

    addr_t ttBaseOffsetGuess=1+callSiteLengthLEBNumBytes+callSiteBuf.len+actionBuf.len+typeTableBuf.len;
    usint ttBaseOffsetNumBytes;
    byte* ttBaseOffsetLEB=encodeAsLEB128((byte*)&ttBaseOffsetGuess,sizeof(ttBaseOffsetGuess),false,&ttBaseOffsetNumBytes);
    //see if there's some wiggle room at the bottom
    bool foundRoom=false;
    for(int idx=ttBaseOffsetNumBytes-1;idx>=0;idx--)
    {
      if(((ttBaseOffsetLEB[idx] &0x7f)!=0x7f && idx>0) ||
         ((ttBaseOffsetLEB[idx] &0x70)!=0x70))
      {
        foundRoom=true;
        break;
      }
    }
    if(!foundRoom)
    {
      //add another byte to the LEB for wiggle room
      ttBaseOffsetLEB=realloc(ttBaseOffsetLEB,ttBaseOffsetNumBytes+1);
      ttBaseOffsetLEB[ttBaseOffsetNumBytes-1] |= 0x80;//to show that it is no
                                             //longer the MSB
      ttBaseOffsetLEB[ttBaseOffsetNumBytes]=0;
      ttBaseOffsetNumBytes++;
    }


    //this is the offset from the beginning of the section to the
    //beginning of the type table
    addr_t offsetStartToTTTop=buf->len-lsdaStartOffset+ttBaseOffsetNumBytes
      +1 //for call site format
      +callSiteLengthLEBNumBytes+callSiteBuf.len
      +actionBuf.len;
    int misalignment=4-(offsetStartToTTTop%4);
    //type table must be 4-byte aligned
    offsetStartToTTTop+=misalignment;
    //to get ttBase, add the length of the type table buf and subtract
    //the number of bytes before the location we write the offset to
    //get a self-relative offset
    addr_t ttBaseOffset=offsetStartToTTTop+typeTableBuf.len-
      (buf->len-lsdaStartOffset)-1;//-1 because points to last entry
                                   //-in table, not below it table it
                                   //-appears (no standard to confirm)
    usint ttBaseOffsetNumBytesNew;
    ttBaseOffsetLEB=encodeAsLEB128((byte*)&ttBaseOffset,sizeof(ttBaseOffset),false,
                                   &ttBaseOffsetNumBytesNew);
    if(ttBaseOffsetNumBytesNew > ttBaseOffsetNumBytes)
    {
      //we added in wiggle room but didn't actually have to use
      //it. We'll just waste a byte
      assert(ttBaseOffsetNumBytesNew-ttBaseOffsetNumBytes==1);
      ttBaseOffsetLEB=realloc(ttBaseOffsetLEB,ttBaseOffsetNumBytes);
      ttBaseOffsetLEB[ttBaseOffsetNumBytes-1] |= 0x80;//to show that it is no
                                             //longer the MSB
      ttBaseOffsetLEB[ttBaseOffsetNumBytes]=0;
    }

    //now we can actually write everything in
    addToGrowingBuffer(buf,ttBaseOffsetLEB,ttBaseOffsetNumBytes);
    byte callSiteFormat=DW_EH_PE_uleb128;
    addToGrowingBuffer(buf,&callSiteFormat,1);
    addToGrowingBuffer(buf,callSiteLengthLEB,callSiteLengthLEBNumBytes);
    addToGrowingBuffer(buf,callSiteBuf.data,callSiteBuf.len);
    free(callSiteBuf.data);
    addToGrowingBuffer(buf,actionBuf.data,actionBuf.len);
    free(actionBuf.data);
    if(misalignment)
    {
      word_t zero=0;
      addToGrowingBuffer(buf,&zero,misalignment);
    }
    addToGrowingBuffer(buf,typeTableBuf.data,typeTableBuf.len);
    free(typeTableBuf.data);
  }//end loop over lsdas for writing
  
}

int compareEhFrameHdrTableEntries(const void* a_,const void* b_)
{
  const EhFrameHdrTableEntry* a=a_;
  const EhFrameHdrTableEntry* b=b_;
  return a->initialLocation-b->initialLocation;
}

//returns a void* to a binary representation of a Dwarf call frame
//information section (i.e. .debug_frame in the dwarf specification)
//the length of the returned buffer is written into byteLen.
//the memory for the buffer should free'd when the caller is finished with it
CallFrameSectionData buildCallFrameSectionData(CallFrameInfo* cfi)
{
  CallFrameSectionData result;
  ZERO(result);
  
  //useless if not an eh_frame
  addr_t* lsdaPointers;

  //if present, deal with .gcc_except_table first because the mapping
  //from addresses/offsets to LSDA locations comes into play when
  //writing FDES
  if(cfi->isEHFrame)
  {
    GrowingBuffer exceptTableBuf;
    ZERO(exceptTableBuf);

    if(cfi->exceptTable)
    {
      buildExceptTableRawData(cfi,&exceptTableBuf,&lsdaPointers);
      result.gccExceptTableData=exceptTableBuf.data;
      result.gccExceptTableLen=exceptTableBuf.len;
    }
  }
  
  GrowingBuffer buf;
  //allocate 1k initially as an arbitrary amount. We'll grow it as needed
  buf.allocated=10000;
  buf.len=0;
  buf.data=malloc(buf.allocated);
  uint* cieOffsets=zmalloc(cfi->numCIEs*sizeof(uint));
  uint* fdeOffsets=zmalloc(cfi->numFDEs*sizeof(uint));
  for(int i=0;i<cfi->numCIEs;i++)
  {
    cieOffsets[i]=buf.len;
    buildCIERawData(cfi->cies+i,&buf,cfi);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    fdeOffsets[i]=buf.len;
    buildFDERawData(cfi,cfi->fdes+i,cieOffsets,lsdaPointers,&buf);
  }
  free(cieOffsets);

  result.ehData=buf.data;
  result.ehDataLen=buf.len;
  
  if(cfi->isEHFrame)
  {
    //build .eh_frame_hdr
    //todo: while this has support for all *common* situations on x86
    //and x86_64, there are situations on x86_64 where it is not
    //necessarily guaranteed to work (although I do not expect them to
    //be encountered in practice)
    GrowingBuffer hdrBuf;//data for .eh_frame_hdr
    memset(&hdrBuf,0,sizeof(hdrBuf));

    //fields taken from LSB at
    //http://refspecs.freestandards.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
    struct
    {
      byte version;
      byte eh_frame_ptr_enc;
      byte fde_count_enc;
      byte table_enc;
      int32 eh_frame_ptr;
      uint32 eh_fde_count;
    } __attribute__((__packed__)) hdrHeader;

    assert(sizeof(hdrHeader)==12);

    hdrHeader.version=EH_FRAME_HDR_VERSION;
    hdrHeader.eh_frame_ptr_enc=DW_EH_PE_sdata4 | DW_EH_PE_pcrel;
    hdrHeader.fde_count_enc=DW_EH_PE_udata4;
    hdrHeader.table_enc=DW_EH_PE_sdata4 | DW_EH_PE_datarel;
    if(0==cfi->ehHdrAddress)
    {
      logprintf(ELL_WARN,ELS_DWARF_BUILD,"The .eh_frame_hdr address appears to be 0. This is almost certainly wrong\n");
    }
    addr_t addressToBeRelativeTo=cfi->ehHdrAddress+4;//4 bytes into it
    int numBytesOut;
    hdrHeader.eh_frame_ptr=
      (int32)encodeEHPointerFromEncoding(cfi->sectionAddress,
                                         hdrHeader.eh_frame_ptr_enc,
                                         addressToBeRelativeTo,
                                         &numBytesOut);
    assert(numBytesOut==4);
    hdrHeader.eh_fde_count=cfi->numFDEs;

    addToGrowingBuffer(&hdrBuf,&hdrHeader,sizeof(hdrHeader));

    //now it's time to add the actual table entries. The LSB standard
    //requires that each entry contains two encoded values: the
    //initial location for an FDE and the address of the FDE. The
    //table must be sorted by initial location
    EhFrameHdrTableEntry* table=zmalloc(sizeof(EhFrameHdrTableEntry)*cfi->numFDEs);
    for(int i=0;i<cfi->numFDEs;i++)
    {
      //remember, encoded as data-relative
      table[i].initialLocation=cfi->fdes[i].lowpc-cfi->ehHdrAddress;
      table[i].fdeAddress=cfi->sectionAddress+fdeOffsets[i]-cfi->ehHdrAddress;
    }
    //sort them
    qsort(table,cfi->numFDEs,sizeof(EhFrameHdrTableEntry),compareEhFrameHdrTableEntries);
    //now actually write them to the buffer
    for(int i=0;i<cfi->numFDEs;i++)
    {
      addToGrowingBuffer(&hdrBuf,&table[i].initialLocation,4);
      addToGrowingBuffer(&hdrBuf,&table[i].fdeAddress,4);
    }
    free(table);
        
    result.ehHdrData=hdrBuf.data;
    result.ehHdrDataLen=hdrBuf.len;
  }
  else
  {
    result.ehHdrData=NULL;
    result.ehHdrDataLen=0;
  }
  return result;
}

//builds an ExceptTable object from the raw ELF section note that the
//format of this section is not governed by any standard, so there is
//no means to validate that what we are doing is correct or even that
//it will work with all binaries emitted by gcc
ExceptTable parseExceptFrame(Elf_Scn* scn,addr_t** lsdaPointers)
{
  ExceptTable result;
  memset(&result,0,sizeof(result));
  Elf_Data* data=elf_getdata(scn,NULL);
  byte* bytes=data->d_buf;
  if(data->d_size == 0)
  {
    return result;
  }

  GElf_Shdr shdr;
  getShdr(scn,&shdr);
  addr_t sectionAddress=shdr.sh_addr;
  
  word_t offset=0;
  while(offset < data->d_size)
  {
    //increase the number of LSDAs
    result.numLSDAs++;
    *lsdaPointers=realloc(*lsdaPointers,sizeof(addr_t)*result.numLSDAs);
    MALLOC_CHECK(*lsdaPointers);
    (*lsdaPointers)[result.numLSDAs-1]=sectionAddress+offset;
    result.lsdas=realloc(result.lsdas,result.numLSDAs*sizeof(LSDA));
    LSDA* lsda=&(result.lsdas[result.numLSDAs-1]);
    memset(lsda,0,sizeof(LSDA));
    
    //start by reading the header
    byte lpStartFormat=bytes[0];
    bytes++;
    offset++;
    usint numBytesOut;
    if(lpStartFormat!=DW_EH_PE_omit)
    {
      lsda->lpStart=decodeEHPointer(bytes,data->d_size-offset,sectionAddress+offset,lpStartFormat,&numBytesOut);
      bytes+=numBytesOut;
      offset+=numBytesOut;
    }
    byte ttFormat=bytes[0];
    bytes++;
    offset++;
    usint numSeptets;
    addr_t ttBase=0;
    byte* ttBaseBytes=decodeLEB128(bytes,false,&numBytesOut,&numSeptets);
    assert(sizeof(ttBase) >= numBytesOut);
    memcpy(&ttBase,ttBaseBytes,numBytesOut);
    free(ttBaseBytes);
    //remember ttBase is a self-relative offset to the end of the type
    //table. For our purposes it's helpful if it is instead relative
    //to the beginning of this section
    ttBase+=offset;
    //ttBase seems to be off by one in what gcc emits. I am not
    //positive whether this is due to it pointing to the last byte of
    //the table rather than being below the table or if it's because
    //it's supposed to be relative to the end of ttBaseBytes. If the
    //later, then this offset is wrong and we will notice a problem
    //later. As before, there is no standard governing this section
    ttBase+=1;
    bytes+=numSeptets;
    offset+=numSeptets;
    byte callSiteFormat=bytes[0];
    bytes++;
    offset++;
    word_t callSiteTableSize=decodeEHPointer(bytes,data->d_size-offset,
                                             sectionAddress+offset,
                                             callSiteFormat,&numBytesOut);
    bytes+=numBytesOut;
    offset+=numBytesOut;

    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table, ran out of space while reading the header\n");
    }
    
    //read the call site table
    CallSiteRecord* callSite=NULL;
    int i=0;
    while(i<callSiteTableSize)
    {
      //set up the call site we're dealing with right now
      lsda->numCallSites++;
      lsda->callSiteTable=realloc(lsda->callSiteTable,lsda->numCallSites*sizeof(CallSiteRecord));
      callSite=&(lsda->callSiteTable[lsda->numCallSites-1]);
      callSite->position=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                         sectionAddress+offset+i,
                                         callSiteFormat,&numBytesOut);
      i+=numBytesOut;
      callSite->length=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                         sectionAddress+offset+i,
                                         callSiteFormat,&numBytesOut);
      i+=numBytesOut;
      callSite->landingPadPosition=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                                   sectionAddress+offset+i,
                                                   callSiteFormat,&numBytesOut);
      i+=numBytesOut;
      callSite->firstAction=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                            sectionAddress+offset+i,
                                            callSiteFormat,&numBytesOut);
      i+=numBytesOut;
      //firstAction is not at all in the form we want. It's one plus a
      //byte offset into the action table. We want an index into the
      //action table we'll read. At the moment, however, we have no
      //way of calculating the index so we'll temporarily store the
      //wrong value in firstAction.
      
      callSite->hasAction=(callSite->firstAction!=0);
      callSite->firstAction-=1;//since it was biased by one so we
                               //could tell if it was empty

      if(i>callSiteTableSize)
      {
        death("Call site table in .gcc_except_frame was not properly formatted\n");
      }
    }
    bytes+=i;
    offset+=i;
    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table, ran out of space while reading the call site table\n");
    }
    
    //read the action table and type table. Now here's the icky part:
    //there is no really easy way to tell where the action table ends
    //and the type table begins. The way gcc is designed, there's no
    //need to ever parse this section directly. Access to the action
    //table is made only based on information in the call site table,
    //and access to the type table is made only based on information
    //in the action table. Therefore, the most reasonable way for us
    //to parse it is probably by following the same logic. Note that
    //we do not optimize, so we may end up parsing the same entry more
    //than once. Big deal.

    //Read the action table
    addr_t highestActionOffset=0;//try to find the end of the action table
    for(int j=0;j<lsda->numCallSites;j++)
    {
      if(!lsda->callSiteTable[j].hasAction)
      {
        continue;
      }
      addr_t actionOffset=lsda->callSiteTable[j].firstAction;

      idx_t previousActionIdx=-1;

      //keep track so we don't set the call site for everything in the
      //chain, which would be wrong
      bool fixedFirstAction=false;

      //follow the action chain for a single call site
      while(true)
      {
        //printf("action offset is 0x%zx\n",actionOffset);
        idx_t typeIdx=leb128ToUWord(bytes+actionOffset,&numBytesOut);
        usint numBytesOut2;
        sword_t nextRecordPtr=leb128ToSWord(bytes+actionOffset+numBytesOut,&numBytesOut2);
        //printf("nextRecordPtr is %i\n",(int)nextRecordPtr);
        if(nextRecordPtr != 0)
        {
          //add numBytesOut to make it relative to the start of the action record
          nextRecordPtr+=numBytesOut;
        }
        highestActionOffset=max(highestActionOffset,actionOffset+numBytesOut+numBytesOut2);
        //see whether we already read this action record
        bool foundAction=false;
        for(int k=0;k<lsda->numActionEntries;k++)
        {
          if(lsda->actionTable[k].typeFilterIndex==typeIdx &&
             lsda->actionTable[k].nextAction==nextRecordPtr)
          {
            //we already read this action

            //make the first action field into the correct form
            lsda->callSiteTable[j].firstAction=k;
            foundAction=true;
            break;
          }
        }
        if(foundAction)
        {
          continue;
        }
        //create a new action record
        lsda->numActionEntries++;
        lsda->actionTable=realloc(lsda->actionTable,sizeof(ActionRecord)*lsda->numActionEntries);
        idx_t actionIdx=lsda->numActionEntries-1;
        memset(lsda->actionTable+actionIdx,0,sizeof(ActionRecord));

        if(!fixedFirstAction)
        {
          //make the first action field into the correct form
          lsda->callSiteTable[j].firstAction=actionIdx;
          fixedFirstAction=true;//otherwise we keep setting it over
                                //again for everything in the chain,
                                //which is wrong
        }
        
        //type indices seem to be 1-based, we make them 0-based
        lsda->actionTable[actionIdx].typeFilterIndex=typeIdx-1;
        //can't set nextAction yet as we don't know the index of the
        //next one. Can set the one for the last though
        if(previousActionIdx!=-1)
        {
          lsda->actionTable[previousActionIdx].nextAction=actionIdx;
        }
        if(nextRecordPtr==0)
        {
          //reached the end of this chain
          lsda->actionTable[actionIdx].hasNextAction=false;
          break;
        }
        else
        {
          lsda->actionTable[actionIdx].hasNextAction=true;
        }
        actionOffset=(sword_t)actionOffset+nextRecordPtr;
        previousActionIdx=actionIdx;
      }//end following the action chain for a specific call stie
    }
    bytes+=highestActionOffset;
    offset+=highestActionOffset;
    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table, ran out of space while reading the action table\n");
    }


    //Read the type table
    //remember the type table grows from larger offsets to smaller
    //offsets, i.e. the smallest index is at the end
    
    //todo: this won't work if LEB encoding used. If we start
    //observing gcc emitting LEB values in the type table we'll need
    //to switch to something else like only going through the type
    //table entries at offsets given in the action table
    int ttEntrySize=getPointerSizeFromEHPointerEncoding(ttFormat);

    idx_t ttIdx=0;
    //printf("ttBase is 0x%zx\n",ttBase);
    //printf("ttEntrySize is 0x%x\n",ttEntrySize);
    //printf("offset is 0x%zx\n",offset);
    for(sword_t ttOffset=ttBase-ttEntrySize-offset;
        ttOffset >= 0;
        ttOffset-=ttEntrySize,ttIdx++)
    {
      //printf("ttOffset is 0x%zx\n",ttOffset);
      lsda->numTypeEntries++;
      lsda->typeTable=realloc(lsda->typeTable,sizeof(word_t)*lsda->numTypeEntries);
      usint numBytesOut;
      lsda->typeTable[ttIdx]=decodeEHPointer(bytes+ttOffset,ttEntrySize,
                                             sectionAddress+offset+ttOffset,
                                             ttFormat,&numBytesOut);
    }
    bytes+=ttBase-offset;
    offset=ttBase;
    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table\n");
    }
  }
  return result;
}
