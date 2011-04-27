/*
  File: eh_pe.c
  Author: James Oakley
  Copyright (C): 2011 Dartmouth College
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
  Description: methods for decoding/encoding ehframe pointer encodings
*/





#include "eh_pe.h"
#include "util/util.h"
#include "leb.h"
#include "util/logging.h"

//16 possible values in a nibble
static char* dwpeApplicationTable[16];
static char* dwpeFormatTable[16];
static bool dwpeTablesBuilt=false;

void buildDWPETables()
{
  dwpeFormatTable[DW_EH_PE_absptr]="DW_EH_PE_absptr";
  dwpeFormatTable[DW_EH_PE_udata2]="DW_EH_PE_udata2";
  dwpeFormatTable[DW_EH_PE_sdata2]="DW_EH_PE_sdata2";
  dwpeFormatTable[DW_EH_PE_udata4]="DW_EH_PE_udata4";
  dwpeFormatTable[DW_EH_PE_sdata4]="DW_EH_PE_sdata4";
  dwpeFormatTable[DW_EH_PE_udata8]="DW_EH_PE_udata8";
  dwpeFormatTable[DW_EH_PE_sdata8]="DW_EH_PE_sdata8";
  dwpeFormatTable[DW_EH_PE_uleb128]="DW_EH_PE_uleb128";
  dwpeFormatTable[DW_EH_PE_sleb128]="DW_EH_PE_sleb128";
  dwpeApplicationTable[DW_EH_PE_pcrel>>4]="DW_EH_PE_pcrel";
  dwpeApplicationTable[DW_EH_PE_textrel>>4]="DW_EH_PE_textrel";
  dwpeApplicationTable[DW_EH_PE_datarel>>4]="DW_EH_PE_datarel";
  dwpeApplicationTable[DW_EH_PE_funcrel>>4]="DW_EH_PE_funcrel";
  dwpeApplicationTable[DW_EH_PE_aligned>>4]="DW_EH_PE_aligned";
  dwpeApplicationTable[DW_EH_PE_indirect>>4 | DW_EH_PE_pcrel>>4]="DW_EH_PE_indirect, DW_EH_PE_pcrel";
  dwpeTablesBuilt=true;
}

void printEHPointerEncoding(FILE* file,byte encoding)
{
  if(!dwpeTablesBuilt)
  {
    buildDWPETables();
  }
  if(encoding==DW_EH_PE_omit)
  {
    fprintf(file,"DW_EH_PE_omit");
    return;
  }
  int format=encoding & 0xF;
  int application=encoding & 0xF0;
  if(!dwpeFormatTable[format])
  {
    death("Trying to print an unknown pointer encoding format 0x%x\n",format);
  }
  if(application)
  {
    if(!dwpeApplicationTable[application>>4])
    {
      death("Trying to print an unknown pointer encoding application 0x%x\n",application);
    }
    fprintf(file,"%s, %s",dwpeFormatTable[format],dwpeApplicationTable[application>>4]);
  }
  else
  {
    fprintf(file,"%s",dwpeFormatTable[format]);
  }
}

//pointer encodings for eh_frame as specified by LSB at
//http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
int getPointerSizeFromEHPointerEncoding(byte encoding)
{
  if(encoding==DW_EH_PE_omit)
  {
    return 0;
  }
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
  //DW_EH_PE_indirect can be ORd with one of the others.  we don't
  //care about it right now, all we have to do is to make sure that if
  //we're told the encoding is indirect that we write it out as
  //indirect again in the end
  switch(application & ~DW_EH_PE_indirect)
  {
  case 0:
    //no special application encoding needed. This does not seem to be
    //properly documented in the LSB, but in practice it seems to be
    //observed.
    return pointer;
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
//dataStartAddress gives the loaded address (from the ELF object) of
//the first byte in data this is necessary for pc-relative
//encoding. bytesRead is the number of bytes actually read
addr_t decodeEHPointer(byte* data,int len,addr_t dataStartAddress,byte encoding,usint* bytesRead)
{
  int application=encoding & 0xF0;
  int format=encoding & 0xF;
  addr_t result=0;
  bool resultSigned=false;
  usint byteSize=0;
  switch(format)
  {
  case DW_EH_PE_absptr:
    memcpy(&result,data,min(len,sizeof(addr_t)));
    byteSize=sizeof(addr_t);
    break;
  case DW_EH_PE_sdata2:
    resultSigned=true;
  case DW_EH_PE_udata2:
    assert(len>=2);
    memcpy(&result,data,2);
    byteSize=2;
    break;
  case DW_EH_PE_sdata4:
    resultSigned=true;
  case DW_EH_PE_udata4:
    assert(len>=4);
    memcpy(&result,data,4);
    byteSize=4;
    break;
  case DW_EH_PE_udata8:
  case DW_EH_PE_sdata8:
    assert(len>=8);
    assert(sizeof(addr_t)>=8);
    memcpy(&result,data,8);
    byteSize=4;
    break;
  case DW_EH_PE_uleb128:
    {
      usint numBytes;
      byte* decodedLEB=decodeLEB128(data,false,&numBytes,&byteSize);
      assert(numBytes<=sizeof(result));
      memcpy(&result,decodedLEB,numBytes);
      free(decodedLEB);
    }
    break;
  case DW_EH_PE_sleb128:
    {
      resultSigned=true;
      usint numBytes;
      byte* decodedLEB=decodeLEB128(data,false,&numBytes,&byteSize);
      assert(numBytes<=sizeof(result));
      memcpy(&result,decodedLEB,numBytes);
      free(decodedLEB);
    }
    break;
  default:
    //todo: implement DW_EH_PE_omit
    death("decodeEHPointer not implemented for unknown eh_frame pointer encoding yet\n");
  }

  if(bytesRead)
  {
    *bytesRead=byteSize;
  }

  //DW_EH_PE_indirect can be ORd with one of the others.  we don't
  //care about it right now, all we have to do is to make sure that if
  //we're told the encoding is indirect that we write it out as
  //indirect again in the end
  
  switch(application & ~DW_EH_PE_indirect)
  {
  case DW_EH_PE_absptr:
    return result;
  case DW_EH_PE_pcrel:
    //todo: do I need to treat signed any differently? I forget the
    //details of how gcc munges arithmetic. Theoretically 2's
    //complement requires no changes to addition since it's just
    //overflow...
    if(!dataStartAddress)
    {
      logprintf(ELL_WARN,ELS_DWARF_FRAME,"decoding pc-relative pointer with null PC. This is probably not what is intended\n");
    }
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
    death("Unknown DW_EH_PE application 0x%x\n",application);
  }
  return result;
}
