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

//implemented in exceptTable.c to make this file a little smaller
void buildExceptTableRawData(CallFrameInfo* cfi,GrowingBuffer* buf,
                             addr_t** lsdaPointers);

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

void parseFDEAugmentationData(FDE* fde,addr_t augDataAddress,byte* augmentationData,int augmentationDataLen,addr_t** lsdaPointers,int* numLSDAPointers)
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
    //store the pointer so the LSDA can be found later
    //first make sure the pointer hasn't already been found
    for(int i=0;i<*numLSDAPointers;i++)
    {
      if(lsdaPointer==(*lsdaPointers)[i])
      {
        fde->lsdaIdx=i;
        return;
      }
    }
    //ok, we have to create a new one
    fde->lsdaIdx=(*numLSDAPointers)++;
    *lsdaPointers=realloc(*lsdaPointers,*numLSDAPointers*sizeof(addr_t));
    (*lsdaPointers)[*numLSDAPointers-1]=lsdaPointer;
    MALLOC_CHECK(*lsdaPointers);
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

