/*
  File: replaceCommand.cpp
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
    
  Project:  Katana
  Date: December 2010
  Description: Command class for the katana shell
*/

#include "replaceCommand.h"
extern "C"
{
#include "elfutil.h"
#include "util/file.h"
}



ReplaceCommand::ReplaceCommand(ReplacementType type,ShellParam* elfObject,ShellParam* which,ShellParam* newThing)
  :type(type),elfObjectP(elfObject),whichThingP(which),newThingP(newThing)
{
  elfObjectP->grab();
  whichThingP->grab();
  newThingP->grab();
}

ReplaceCommand::~ReplaceCommand()
{
  elfObjectP->drop();
  whichThingP->drop();
  newThingP->drop();
}

void ReplaceCommand::execute()
{
  ElfInfo* e=this->elfObjectP->getElfObject();
  switch(this->type)
  {
  case RT_SECTION:
    {
      char* whichSectionName=whichThingP->getString();
      Elf_Scn* scn=getSectionByName(e,whichSectionName);
      if(!scn)
      {
        logprintf(ELL_WARN,ELS_SHELL,"Unable to find section '%s' when trying to execute replace section commnad\n",whichSectionName);
        throw "Cannot replace section";
      }
      Elf_Data* dataForSection=elf_getdata(scn,NULL);
      int dataLen=0;
      void* newData=newThingP->getRawData(&dataLen);
      replaceScnData(dataForSection,newData,dataLen);
      logprintf(ELL_INFO_V2,ELS_SHELL,"Replaced section \"%s\"\n",whichSectionName);
      if(newThingP->isCapable(SPC_SECTION_HEADER))
      {
        //the newThing is capable of giving us a new section header as well as new data
        GElf_Shdr shdr;
        //first get the existing section header
        getShdr(scn,&shdr);
        //then update it
        SectionHeaderData* headerData=newThingP->getSectionHeader();
        updateShdrFromSectionHeaderData(e,headerData,&shdr);
        gelf_update_shdr(scn,&shdr);
        elf_flagshdr(scn,ELF_C_SET,ELF_F_DIRTY);
      }
    }
    break;
  case RT_RAW:
    {
      if(!whichThingP->isCapable(SPC_INT_VALUE))
      {
        logprintf(ELL_WARN,ELS_SHELL,"replace raw must be given a file offset at which to replace bytes\n");
        throw "Unable to perform replace raw command";
      }
      word_t offset=whichThingP->getInt();
      byte* rawBytes=NULL;
      int rawBytesLen;
      bool shouldFreeRawBytes=false;
      if(newThingP->isCapable(SPC_RAW_DATA))
      {
        rawBytes=newThingP->getRawData(&rawBytesLen);
      }
      else if(newThingP->isCapable(SPC_STRING_VALUE))
      {
        //treat the string as a filename
        char* filename=newThingP->getString();
        rawBytes=(byte*)getFileContents(filename,&rawBytesLen);
      }
      else
      {
        throw "Unable to get raw data from parameter";
      }

      Elf_Scn* closestScn=NULL;
      GElf_Shdr closestScnShdr;
      
      //now the tough thing to do is to find the right section to
      //insert this data into
      for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
      {
        GElf_Shdr shdr;
        gelf_getshdr(scn,&shdr);
        if(offset > shdr.sh_offset)
        {
          if(!closestScn || shdr.sh_offset > closestScnShdr.sh_offset)
          {
            //we've found a section we might be able to extend to get
            //to the offset we need
            closestScn=scn;
            closestScnShdr=shdr;
            if(shdr.sh_offset + shdr.sh_size > offset+rawBytesLen)
            {
              //the data we want to place fits exactly within the section
              break;
            }
          }
        }
      }
      if(!closestScn)
      {
        logprintf(ELL_WARN,ELS_SHELL,"Unable to find any ELF section in which to replace from file offset 0x%zx to 0x%zx\n",offset,offset+rawBytesLen);
        throw "Unable to replace raw data\n";
      }
      
      modifyScnData(elf_getdata(closestScn,NULL),offset-closestScnShdr.sh_offset,rawBytes,rawBytesLen);
      logprintf(ELL_INFO_V2,ELS_SHELL,"Replaced %i bytes in section %s\n",
                rawBytesLen,
                getSectionNameFromIdx(e,elf_ndxscn(closestScn)));

      if(shouldFreeRawBytes)
      {
        free(rawBytes);
      }
        
    }
    break;
  default:
    logprintf(ELL_ERR,ELS_SHELL,"Unhandled replacement type\n");
  }

}
