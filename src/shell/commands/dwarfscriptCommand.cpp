/*
  File: dwarfscriptCommand.cpp
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

#include "dwarfscriptCommand.h"
extern "C"
{
#include "fderead.h"
#include "shell/dwarfscript/dwarfscript.yy.h"
  
extern int yydwdebug;
extern FILE *yydwin;
extern int yydwparse();
extern CallFrameInfo* parsedCallFrameInfo;
}

//constructor for compile operation
DwarfscriptCommand::DwarfscriptCommand(DwarfscriptOperation op,ShellParam* input,ShellParam* outfile)
  :op(op),inputP(input),outfileP(outfile)
{
  assert(op==DWOP_COMPILE);
  inputP->grab();
  if(outfileP)
  {
    outfileP->grab();
  }
}
//constructor for emit operation
DwarfscriptCommand::DwarfscriptCommand(DwarfscriptOperation op,ShellParam* sectionName,ShellParam* elfObject,ShellParam* outfile)
  :op(op),sectionNameP(sectionName),elfObjectP(elfObject),outfileP(outfile)
{
  sectionNameP->grab();
  elfObjectP->grab();
  outfileP->grab();
  assert(op==DWOP_EMIT);
}

DwarfscriptCommand::~DwarfscriptCommand()
{
  if(inputP)
  {
    inputP->drop();
  }
  if(sectionNameP)
  {
    sectionNameP->drop();
  }
  if(elfObjectP)
  {
    elfObjectP->drop();
  }
  if(outfileP)
  {
    outfileP->drop();
  }
}

void DwarfscriptCommand::printCIEInfo(FILE* file,CIE* cie)
{
  
  fprintf(file,"begin CIE\n");
  fprintf(file,"index: %i\n",cie->idx);
  fprintf(file,"version: %i\n",cie->version);
  fprintf(file,"data_align: %li\n",(long int)cie->dataAlign);
  fprintf(file,"code_align: %li\n",(long int)cie->dataAlign);
  fprintf(file,"return_addr_rule: %li\n",(long int)cie->returnAddrRuleNum);
  fprintf(file,"augmentation: \"%s\"\n",cie->augmentation);
  if(cie->augmentationDataLen)
  {
    char* str=getHexDataString(cie->augmentationData,cie->augmentationDataLen);
    fprintf(file,"augmentation_data: hex;%s\n",str);
    free(str);
  }
  fprintf(file,"begin INSTRUCTIONS\n");
  for(int i=0;i<cie->numInitialInstructions;i++)
  {
    printInstruction(file,cie->initialInstructions[i],DWIPF_NO_REG_NAMES);
  }
  fprintf(file,"end INSTRUCTIONS\n");
  fprintf(file,"end CIE\n");
}

void DwarfscriptCommand::printFDEInfo(FILE* file,FDE* fde)
{
  fprintf(file,"begin FDE\n");
  fprintf(file,"index: %i\n",fde->idx);
  fprintf(file,"cie_index: %i\n",fde->cie->idx);
  fprintf(file,"initial_location: 0x%x\n",fde->lowpc);
  fprintf(file,"address_range: 0x%x\n",fde->highpc-fde->lowpc);
  if(fde->augmentationDataLen)
  {
    char* str=getHexDataString(fde->augmentationData,fde->augmentationDataLen);
    fprintf(file,"augmentation_data: hex;%s\n",str);
    free(str);
  }
  fprintf(file,"begin INSTRUCTIONS\n");
  for(int i=0;i<fde->numInstructions;i++)
  {
    printInstruction(file,fde->instructions[i],DWIPF_NO_REG_NAMES);
  }
  fprintf(file,"end INSTRUCTIONS\n");
  fprintf(file,"end FDE\n");

}


void DwarfscriptCommand::emitDwarfscript()
{
  ElfInfo* elf=elfObjectP->getElfObject();
  char* sectionName=sectionNameP->getString();
  if(!sectionName)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Cannot emit dwarfscript because section name parameter was not a string\n");
    return;
  }
  char* outfileName=NULL;
  if(outfileP)
  {
    outfileName=outfileP->getString();
    if(!outfileName)
    {
      logprintf(ELL_WARN,ELS_SHELL,"Cannot emit dwarfscript because outfile name parameter was not a string\n");
      return;
    }
  }

  //todo: handle case where no outfile
  
  if(!strcmp(sectionName,".eh_frame"))
  {
    readDebugFrame(elf,true);
  }
  else if(!strcmp(sectionName,".debug_frame"))
  {
    readDebugFrame(elf,false);
  }
  else
  {
    logprintf(ELL_WARN,ELS_SHELL,"Cannot emit dwarfscript for that ELF section\n");
    return;
  }
  FILE* file=fopen(outfileName,"w");
  if(!file)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Cannot open file %s for writing dwarfscript to\n",outfileName);
    return;
  }
  CallFrameInfo* cfi=&elf->callFrameInfo;
  for(int i=0;i<cfi->numCIEs;i++)
  {
    this->printCIEInfo(file,cfi->cies+i);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    this->printFDEInfo(file,cfi->fdes+i);
  }
  fclose(file);
  logprintf(ELL_INFO_V2,ELS_SHELL,"Wrote dwarfscript to %s\n",outfileName);
  
}

void DwarfscriptCommand::compileDwarfscript()
{
  char* infileName=inputP->getString();
  if(!infileName)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Cannot compile dwarfscript because infile name parameter was not a string\n");
    return;
  }
  FILE* infile=fopen(infileName,"r");
  if(!infile)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Cannot open file %s for reading dwarfscript from\n",infileName);
    return;
  }
  //yydwdebug=1;
  yydwin=infile;
  memset(parsedCallFrameInfo,0,sizeof(CallFrameInfo));
  bool parseSuccess = (0==yydwparse());
  if(!parseSuccess)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Unable to parse dwarfscript input\n");
    return;
  }

  int dataLen;
  void* data=buildCallFrameSectionData(parsedCallFrameInfo,&dataLen);
  if(outfileP)
  {
    char* outfileName=outfileP->getString();
    if(outfileName)
    {
      FILE* outfile=fopen(outfileName,"w");
      if(outfile)
      {
        fwrite(data,dataLen,1,outfile);
        fclose(outfile);
        logprintf(ELL_INFO_V2,ELS_SHELL,"Wrote compiled dwarfscript call frame data to  file \"%s\" \n",outfileName);
      }
      else
      {
        logprintf(ELL_WARN,ELS_SHELL,"Cannot open file %s for writing DWARF call frame data to\n",outfileName);
      }
    }
    else
    {
      logprintf(ELL_WARN,ELS_SHELL,"Cannot write compiled dwarfscript out because outfile name parameter was not a string\n");
    }
  }
}


void DwarfscriptCommand::execute()
{
  switch(this->op)
  {
  case DWOP_EMIT:
    this->emitDwarfscript();
    break;
  case DWOP_COMPILE:
    this->compileDwarfscript();
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"Unhandled dwarfscript operation\n");
  }
}
