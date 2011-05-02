/*
  File: extractCommand.cpp
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
    
  Project:  Katana
  Date: May 2011
  Description: Command class for the katana shell. This command allows
  extracting various data from ELF objects
*/


#include "extractCommand.h"
extern "C"
{
#include "elfutil.h"
}

SectionHeaderData gshdrToSectionHeaderData(ElfInfo* e,const GElf_Shdr& shdr)
{
  SectionHeaderData shd;
  char* scnName=getScnHdrString(e,shdr.sh_name);
  if(strlen(scnName)>sizeof(shd.name))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Section name %s too long, will be truncated in extract command\n",scnName);
  }
  strncpy(shd.name,scnName,sizeof(shd.name)-1);

  shd.sh_type=shdr.sh_type;
  shd.sh_flags=shdr.sh_flags;
  shd.sh_addr=shdr.sh_addr;
  shd.sh_offset=shdr.sh_offset;
  shd.sh_size=shdr.sh_size;
  shd.sh_link=shdr.sh_link;
  shd.sh_info=shdr.sh_info;
  shd.sh_addralign=shdr.sh_addralign;
  shd.sh_entsize=shdr.sh_entsize; 
  return shd;
}

ExtractCommand::ExtractCommand(ExtractType type,ShellParam* elfObject,ShellParam* whichThing)
  :type(type),elfObjectP(elfObject),whichThingP(whichThing)
{
  elfObjectP->grab();
  whichThingP->grab();
}
ExtractCommand::~ExtractCommand()
{
  elfObjectP->drop();
  whichThingP->drop();
}
void ExtractCommand::execute()
{
  if(!this->outputVariable)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Extract command given with no target, doing nothing\n");
    return;
  }
  ElfInfo* e=this->elfObjectP->getElfObject();
  if(!e)
  {
    throw "invalid ELF for extract command";
  }
  switch(type)
  {
  case ET_SECTION:
    char* whichSectionName=whichThingP->getString();
    Elf_Scn* scn=getSectionByName(e,whichSectionName);
    if(!scn)
    {
      logprintf(ELL_WARN,ELS_SHELL,"Unable to find section '%s' when trying to execute extract section commnad\n",whichSectionName);
      throw "Cannot extract section";
    }
    Elf_Data* dataForSection=elf_getdata(scn,NULL);
    GElf_Shdr shdr;
    getShdr(scn,&shdr);
    SectionHeaderData shd=gshdrToSectionHeaderData(e,shdr);
    this->outputVariable->setValue((byte*)dataForSection->d_buf,dataForSection->d_size,&shd);
  }
}
