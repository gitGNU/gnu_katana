/*
  File: callFrameInfo.c
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
  Date: December 2010
  Description: Data structures and methods dealing with call frame information
*/

#include "callFrameInfo.h"
#include "constants.h"
#include "leb.h"
#include "util/logging.h"

void printEHPointerEncoding(FILE* file,byte encoding)
{
  static char* applicationTable[16];
  static char* formatTable[16];
  static bool tablesBuilt=false;
  if(!tablesBuilt)
  {
    formatTable[DW_EH_PE_absptr]="DW_EH_PE_absptr";
    formatTable[DW_EH_PE_udata2]="DW_EH_PE_udata2";
    formatTable[DW_EH_PE_sdata2]="DW_EH_PE_sdata2";
    formatTable[DW_EH_PE_udata4]="DW_EH_PE_udata4";
    formatTable[DW_EH_PE_sdata4]="DW_EH_PE_sdata4";
    formatTable[DW_EH_PE_udata8]="DW_EH_PE_udata8";
    formatTable[DW_EH_PE_sdata8]="DW_EH_PE_sdata8";
    formatTable[DW_EH_PE_uleb128]="DW_EH_PE_uleb128";
    formatTable[DW_EH_PE_sleb128]="DW_EH_PE_sleb128";
    applicationTable[DW_EH_PE_pcrel>>4]="DW_EH_PE_pcrel";
    applicationTable[DW_EH_PE_textrel>>4]="DW_EH_PE_textrel";
    applicationTable[DW_EH_PE_datarel>>4]="DW_EH_PE_datarel";
    applicationTable[DW_EH_PE_funcrel>>4]="DW_EH_PE_funcrel";
    applicationTable[DW_EH_PE_aligned>>4]="DW_EH_PE_aligned";
  }
  int format=encoding & 0xF;
  int application=encoding & 0xF0;
  fprintf(file,"%s, %s",formatTable[format],applicationTable[application>>4]);
}

//pointer encodings for eh_frame as specified by LSB at
//http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
int getPointerSizeFromEHPointerEncoding(byte encoding)
{
  int format=encoding & 0xF;
  switch(format)
  {
  case DW_EH_PE_absptr:
    return sizeof(addr_t);
    break;
  case DW_EH_PE_udata2:
  case DW_EH_PE_sdata2:
    return 2;
  case DW_EH_PE_udata4:
  case DW_EH_PE_sdata4:
    return 4;
  case DW_EH_PE_udata8:
  case DW_EH_PE_sdata8:
    return 8;
  case DW_EH_PE_uleb128:
  case DW_EH_PE_sleb128:
    death("getPointerSize not implemented for LEB encodings yet\n");
  default:
    death("getPointerSize not implemented for unknown eh_frame pointer encoding yet\n");
  }
  return 0;
}

//encodes a pointer for writing to .eh_frame. The number of bytes of
//the returned address that should be used is indicated by numBytesOut
//the location of the pointer is included for PC-relative addresses
//pointer encodings for eh_frame as specified by LSB at
//http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
addr_t encodeEHPointerFromEncoding(addr_t pointer,byte encoding,
                                   addr_t pointerLocation,int* numBytesOut)
{
  *numBytesOut=getPointerSizeFromEHPointerEncoding(encoding);
  int application=encoding & 0xF0;
  switch(application)
  {
  case 0:
    //no special application encoding needed. This does not seem to be
    //properly documented in the LSB, but in practice it seems to be
    //observed.
    break;
  case DW_EH_PE_pcrel:
    return pointer-pointerLocation;
    break;
  case DW_EH_PE_textrel:
    death("DW_EH_PE_textrel not implemented yet");
    break;
  case DW_EH_PE_datarel:
    death("DW_EH_PE_datarel not implemented yet");
    break;
  case DW_EH_PE_funcrel:
    death("DW_EH_PE_funcrel not implemented yet");
    break;
  case DW_EH_PE_aligned:
    death("DW_EH_PE_aligned not implemented yet");
    break;
  default:
    death("Unknown eh_frame pointer encoding\n");
    //todo: implement DW_EH_PE_omit
  }
  return 0;
}

//decode an eh_frame pointer from the given data. The data pointer
//must point to an area of data at least len bytes long. The pointer
//will not necessarily occupy all of these len bytes.
//dataStartAddress gives the loaded address (from the ELF object) of the first byte in data
//this is necessary for pc-relative encoding
addr_t decodeEHPointer(byte* data,int len,addr_t dataStartAddress,byte encoding)
{
  int application=encoding & 0xF0;
  int format=encoding & 0xF;
  addr_t result=0;
  bool resultSigned=false;
  switch(format)
  {
  case DW_EH_PE_absptr:
    memcpy(&result,data,min(len,sizeof(addr_t)));
    break;
  case DW_EH_PE_sdata2:
    resultSigned=true;
  case DW_EH_PE_udata2:
    assert(len>=2);
    memcpy(&result,data,2);
    break;
  case DW_EH_PE_sdata4:
    resultSigned=true;
  case DW_EH_PE_udata4:
    assert(len>=4);
    memcpy(&result,data,4);
    break;
  case DW_EH_PE_udata8:
  case DW_EH_PE_sdata8:
    assert(len>=8);
    assert(sizeof(addr_t)>=8);
    memcpy(&result,data,8);
    break;
  case DW_EH_PE_uleb128:
    {
      usint numBytes,numSeptets;
      byte* decodedLEB=decodeLEB128(data,false,&numBytes,&numSeptets);
      assert(numBytes<=sizeof(result));
      memcpy(&result,decodedLEB,numBytes);
      free(decodedLEB);
    }
    break;
  case DW_EH_PE_sleb128:
    {
      resultSigned=true;
      usint numBytes,numSeptets;
      byte* decodedLEB=decodeLEB128(data,false,&numBytes,&numSeptets);
      assert(numBytes<=sizeof(result));
      memcpy(&result,decodedLEB,numBytes);
      free(decodedLEB);
    }
    break;
  default:
    //todo: implement DW_EH_PE_omit
    death("decodeEHPointer not implemented for unknown eh_frame pointer encoding yet\n");
  }


  
  switch(application)
  {
  case DW_EH_PE_pcrel:
    //todo: do I need to treat signed any differently? I forget the
    //details of how gcc munges arithmetic. Theoretically 2's
    //complement requires no changes to addition since it's just
    //overflow...
    return result+dataStartAddress;
    break;
  case DW_EH_PE_textrel:
    death("DW_EH_PE_textrel not implemented yet");
    break;
  case DW_EH_PE_datarel:
    death("DW_EH_PE_datarel not implemented yet");
    break;
  case DW_EH_PE_funcrel:
    death("DW_EH_PE_funcrel not implemented yet");
    break;
  case DW_EH_PE_aligned:
    death("DW_EH_PE_aligned not implemented yet");
    break;
  default:
    //no standard states it, but examing both code used in libdwarf and code
    //used in gcc (see the function read_encoded_value_with_base in gcc/unwind-pe.h)
    //reveals that normal behaviour seems to be to apply no special
    //offset if the application bits of format are 0. In practice this
    //behaviour seems to be necessary base don what gcc emits.
    return result;
  }
  return result;
}



//reads the CIE's augmentationData and tries to set up
//the cie->augmentationInfo
//see the Linux Standards Base at
//http://refspecs.freestandards.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
//for augmentation data information
void parseAugmentationStringAndData(CIE* cie)
{
  if(!cie->augmentation)
  {
    return;
  }
  int cnt=strlen(cie->augmentation);
  int offset=0;
  for(int i=0;i<cnt;i++)
  {
    assert(offset<cie->augmentationDataLen);
    char c=cie->augmentation[i];
    switch(c)
    {
    case 'z':
      cie->augmentationInfo.flags|=CAF_DATA_PRESENT;
      break;
    case 'R':
      cie->augmentationInfo.flags|=CAF_FDE_ENC;
      cie->augmentationInfo.fdePointerEncoding=cie->augmentationData[offset];
      offset++;
      break;
    case 'P':
      {
        int len=getPointerSizeFromEHPointerEncoding(cie->augmentationData[offset]);
        offset+=1+len;
      }
      break;
    case 'L':
      cie->augmentationInfo.flags|=CAF_FDE_LSDA;
      cie->augmentationInfo.fdeLSDAPointerEncoding=cie->augmentationData[offset];
      offset++;
    default:
      break;
    }
  }
}

void parseFDEAugmentationData(FDE* fde,addr_t augDataAddress)
{
  //todo: are there any other types of augmentation we may encounter?
  if(fde->cie->augmentationInfo.flags & CAF_FDE_LSDA)
  {
    if(fde->augmentationDataLen <
       getPointerSizeFromEHPointerEncoding(fde->cie->augmentationInfo.fdeLSDAPointerEncoding))
    {
      death("Mismatch between CIE and FDE: FDE does not have expected LSDA pointer augmentation data\n");
    }
    fde->augmentationInfo.LSDAPointer=
      decodeEHPointer(fde->augmentationData,
                      fde->augmentationDataLen,
                      augDataAddress,
                      fde->cie->augmentationInfo.fdeLSDAPointerEncoding);
    
  }
  fde->augmentationInfo.filled=true;
}

void buildCIERawData(CIE* cie,GrowingBuffer* buf,bool isEHFrame)
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
  header1.CIE_id=isEHFrame?EH_CIE_ID:DEBUG_CIE_ID;
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

  byte* augmentationDataLength;
  usint augmentationDataLengthLen=0;
  //from the Linux Standard's Base
  //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  if(cie->augmentationInfo.flags & CAF_DATA_PRESENT)
  {
    augmentationDataLength=uintToLEB128(cie->augmentationDataLen,&augmentationDataLengthLen);
  }
  

  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(cie->initialInstructions,
                                  cie->numInitialInstructions);

  header1.length=sizeof(header1)-sizeof(header1.length)+strlen(cie->augmentation)+1+codeAlignLen+dataAlignLen+returnAddrRegLen+cie->augmentationDataLen+augmentationDataLengthLen+rawInstructions.numBytes;
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
  addToGrowingBuffer(buf,cie->augmentation,strlen(cie->augmentation)+1);
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
  if(cie->augmentationDataLen)
  {
    addToGrowingBuffer(buf,cie->augmentationData,cie->augmentationDataLen);
  }
  addToGrowingBuffer(buf,rawInstructions.instrs,rawInstructions.numBytes);
  addToGrowingBuffer(buf,padding,paddingNeeded);
  destroyRawInstructions(rawInstructions);
}

void buildFDERawData(CallFrameInfo* cfi,FDE* fde,uint* cieOffsets,GrowingBuffer* buf)
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
                                  fde->cie->augmentationInfo.fdePointerEncoding,
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

  byte* augmentationDataLength;
  usint augmentationDataLengthLen=0;
  //from the Linux Standard's Base
  //http://refspecs.freestandards.org/LSB_3.0.0/LSB-Core-generic/LSB-Core-generic/ehframechpt.html
  if(fde->cie->augmentationInfo.flags & CAF_DATA_PRESENT)
  {
    augmentationDataLength=uintToLEB128(fde->augmentationDataLen,&augmentationDataLengthLen);
  }

  //now we finially have enough information to compute the length and padding
  header.length=sizeof(header)-sizeof(header.length)+initialLocationByteLen+
    addressRangeByteLen+augmentationDataLengthLen+fde->augmentationDataLen+
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
  parseFDEAugmentationData(fde,cfi->sectionAddress+buf->len);
  if(fde->cie->augmentationInfo.flags & CAF_DATA_PRESENT)
  {
    addToGrowingBuffer(buf,augmentationDataLength,augmentationDataLengthLen);
    if(fde->augmentationDataLen)
    {
      addToGrowingBuffer(buf,fde->augmentationData,fde->augmentationDataLen);
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
    buildCIERawData(cfi->cies+i,&buf,cfi->isEHFrame);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    fdeOffsets[i]=buf.len;
    buildFDERawData(cfi,cfi->fdes+i,cieOffsets,&buf);
  }
  free(cieOffsets);

  CallFrameSectionData result;
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
