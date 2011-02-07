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
#include "shell/variableTypes/rawVariableData.h"
extern "C"
{
#include "fderead.h"
#include "shell/dwarfscript/dwarfscript.yy.h"
#include "elfutil.h"
#include "eh_pe.h"
  
extern int yydwdebug;
extern FILE *yydwin;
extern int yydwparse();
extern CallFrameInfo* parsedCallFrameInfo;
}

//constructor for compile operation
DwarfscriptCommand::DwarfscriptCommand(DwarfscriptOperation op,ShellParam* input,ShellParam* outfile)
  :op(op),inputP(input),outfileP(outfile)
{
  sectionNameP=NULL;
  elfObjectP=NULL;
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
  inputP=NULL;
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
  fprintf(file,"code_align: %li\n",(long int)cie->codeAlign);
  fprintf(file,"return_addr_rule: %li\n",(long int)cie->returnAddrRuleNum);
  if(cie->augmentationFlags & CAF_FDE_ENC)
  {
    fprintf(file,"fde_ptr_enc: ");
    printEHPointerEncoding(file,cie->fdePointerEncoding);
    fprintf(file,"\n");
  }
  if(cie->augmentationFlags & CAF_FDE_LSDA)
  {
    fprintf(file,"fde_lsda_ptr_enc: ");
    printEHPointerEncoding(file,cie->fdeLSDAPointerEncoding);
    fprintf(file,"\n");
  }
  if(cie->augmentationFlags & CAF_PERSONALITY)
  {
    fprintf(file,"personality_ptr_enc: ");
    printEHPointerEncoding(file,cie->personalityPointerEncoding);
    fprintf(file,"\npersonality: 0x%zx\n",cie->personalityFunction);
  }
  fprintf(file,"begin INSTRUCTIONS\n");
  for(int i=0;i<cie->numInitialInstructions;i++)
  {
    printInstruction(file,cie->initialInstructions[i],DWIPF_NO_REG_NAMES);
  }
  fprintf(file,"end INSTRUCTIONS\n");
  fprintf(file,"end CIE\n");
}

void DwarfscriptCommand::printFDEInfo(FILE* file,ElfInfo* elf,FDE* fde)
{
  fprintf(file,"#function guess: %s\n",getFunctionNameAtPC(elf,fde->lowpc));
  fprintf(file,"begin FDE\n");
  fprintf(file,"index: %i\n",fde->idx);
  fprintf(file,"cie_index: %i\n",fde->cie->idx);
  fprintf(file,"initial_location: 0x%x\n",fde->lowpc);
  fprintf(file,"address_range: 0x%x\n",fde->highpc-fde->lowpc);
  if(fde->hasLSDAPointer)
  {
    fprintf(file,"lsda_idx: 0x%zx\n",fde->lsdaIdx);
  }
  fprintf(file,"begin INSTRUCTIONS\n");
  for(int i=0;i<fde->numInstructions;i++)
  {
    printInstruction(file,fde->instructions[i],DWIPF_NO_REG_NAMES|DWIPF_DWARFSCRIPT);
  }
  fprintf(file,"end INSTRUCTIONS\n");
  fprintf(file,"end FDE\n");

}

void DwarfscriptCommand::printExceptTableInfo(FILE* file,ElfInfo* elf,ExceptTable* et)
{
  for(int i=0;i<et->numLSDAs;i++)
  {
    LSDA* lsda=et->lsdas+i;
    fprintf(file,"#LSDA %i\n",i);
    fprintf(file,"begin LSDA\n");
    fprintf(file,"lpstart: 0x%zx\n",lsda->lpStart);
    for(int j=0;j<lsda->numCallSites;j++)
    {
      CallSiteRecord* callSite=lsda->callSiteTable+j;
      fprintf(file,"#call site %i\n",j);
      fprintf(file,"begin CALL_SITE\n");
      fprintf(file,"position: 0x%zx\n",callSite->position);
      fprintf(file,"length: 0x%zx\n",callSite->length);
      fprintf(file,"landing_pad: 0x%zx\n",callSite->landingPadPosition);
      fprintf(file,"has_action: %s\n",callSite->hasAction?"true":"false");
      if(callSite->hasAction)
      {
        fprintf(file,"first_action: %zi\n",callSite->firstAction);
      }
      fprintf(file,"end CALL_SITE\n");
    }
    for(int j=0;j<lsda->numActionEntries;j++)
    {
      fprintf(file,"#action %i\n",j);
      fprintf(file,"begin ACTION\n");
      fprintf(file,"type_idx: %zi\n",lsda->actionTable[j].typeFilterIndex);
      if(lsda->actionTable[j].hasNextAction)
      {
        fprintf(file,"next: %zi\n",lsda->actionTable[j].nextAction);
      }
      else
      {
        fprintf(file,"next: none\n");
      }
      fprintf(file,"end ACTION\n");
    }
    for(int j=0;j<lsda->numTypeEntries;j++)
    {
      fprintf(file,"#type entry %i\n",j);
      fprintf(file,"typeinfo: 0x%zx\n",lsda->typeTable[j]);
    }
    fprintf(file,"end LSDA\n");
  }
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
  fprintf(file,"section_type: \"%s\"\n",cfi->isEHFrame?".eh_frame":".debug_frame");
  fprintf(file,"section_addr: 0x%zx\n",cfi->sectionAddress);
  if(cfi->isEHFrame)
  {
    fprintf(file,"eh_hdr_addr: 0x%zx\n",cfi->ehHdrAddress);
    fprintf(file,"except_table_addr: 0x%zx\n",cfi->exceptTableAddress);
  }
  for(int i=0;i<cfi->numCIEs;i++)
  {
    this->printCIEInfo(file,cfi->cies+i);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    this->printFDEInfo(file,elf,cfi->fdes+i);
  }
  if(cfi->exceptTable)
  {
    this->printExceptTableInfo(file,elf,cfi->exceptTable);
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
    throw "Unable to compile dwarfscript";
    return;
  }
  //yydwdebug=1;
  yydwin=infile;
  memset(parsedCallFrameInfo,0,sizeof(CallFrameInfo));
  bool parseSuccess = (0==yydwparse());
  if(!parseSuccess)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Unable to parse dwarfscript input\n");
    throw "Unable to compile dwarfscript";
    return;
  }
  

  CallFrameSectionData data=buildCallFrameSectionData(parsedCallFrameInfo);
  if(outfileP)
  {
    char* outfileName=outfileP->getString();
    if(outfileName)
    {
      FILE* outfile=fopen(outfileName,"w");
      if(outfile)
      {
        if(data.ehHdrData)
        {
          fwrite(data.ehHdrData,data.ehHdrDataLen,1,outfile);
        }
        fwrite(data.ehData,data.ehDataLen,1,outfile);
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


  if(this->outputVariable)
  {
    //make an array with .eh_frame_hdr and .eh_frame items
    ShellVariableData** items=(ShellVariableData**)zmalloc(sizeof(ShellVariableData*)*3);
    items[0]=new ShellRawVariableData(data.ehData,data.ehDataLen);
    items[1]=new ShellRawVariableData(data.ehHdrData,data.ehHdrDataLen);
    items[2]=new ShellRawVariableData(data.gccExceptTableData,data.gccExceptTableLen);
    this->outputVariable->makeArray(items,3);
    free(items);
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
