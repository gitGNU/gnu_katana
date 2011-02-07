/*
  File: eh_pe.h
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




#ifndef eh_pe_h
#define eh_pe_h

//decode an eh_frame pointer from the given data. The data pointer
//must point to an area of data at least len bytes long. The pointer
//will not necessarily occupy all of these len bytes.
//dataStartAddress gives the loaded address (from the ELF object) of
//the first byte in data this is necessary for pc-relative
//encoding. bytesRead is the number of bytes actually read
addr_t decodeEHPointer(byte* data,int len,addr_t dataStartAddress,byte encoding,usint* bytesRead);

//encodes a pointer for writing to .eh_frame. The number of bytes of
//the returned address that should be used is indicated by numBytesOut
//the location of the pointer is included for PC-relative addresses
//pointer encodings for eh_frame as specified by LSB at
//http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
addr_t encodeEHPointerFromEncoding(addr_t pointer,byte encoding,
                                   addr_t pointerLocation,int* numBytesOut);

void printEHPointerEncoding(FILE* file,byte encoding);

//pointer encodings for eh_frame as specified by LSB at
//http://refspecs.freestandards.org/LSB_3.1.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html#DWARFEHENCODING
int getPointerSizeFromEHPointerEncoding(byte encoding);

#endif
