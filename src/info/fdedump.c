/*
  File: fdedump.c
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
  Date: February 2010
  Description: Procedures to print out information about the fdes contained in an ELF file,
               specifically geared towards printing out FDEs in Patch Objects with
               Katana's special registers and operations
*/

#include <assert.h>
#include "fderead.h"
#include "dwarfvm.h"
#include "dwarf_instr.h"
#include "dwarftypes.h"
#include "elfutil.h"

Dwarf_Debug dbgForFDEDump;

//The difference between this function and
//DwarfscriptCommand::printCIEInfo is that this function is geared
//towards displaying information to a human and the other is geared
//towards creating commands in a scripting language. While there may
//be some duplication not strictly necessary, neither function is
//difficult or complicated.
void printCIEInfo(FILE* file,CIE* cie)
{
  fprintf(file,"------CIE-----\n");
  fprintf(file,"\tdata align: %i\n",(int)cie->dataAlign);
  fprintf(file,"\tcode align: %u\n",(unsigned int)cie->codeAlign);
  fprintf(file,"\trule for return address: %i\n",cie->returnAddrRuleNum);
  int flags=cie->augmentationFlags;
  if(flags & CAF_DATA_PRESENT)
  {
    fprintf(file,"\taugmentation: \"z");
    flags & CAF_PERSONALITY ? fprintf(file,"P"):false/*nop*/;
    flags & CAF_FDE_LSDA ? fprintf(file,"L"):false/*nop*/;
    flags & CAF_FDE_ENC ? fprintf(file,"R"):false/*nop*/;
    fprintf(file,"\"\n");
    if(flags & CAF_PERSONALITY)
    {
      fprintf(file,"Personality routine: 0x%zx\n",cie->personalityFunction);
    }
    if(flags & CAF_FDE_ENC)
    {
      fprintf(file,"\tFDE pointer encoding:");
      printEHPointerEncoding(file,cie->fdePointerEncoding);
      fprintf(file,"\n");
    }
    if(flags & CAF_FDE_LSDA)
    {
      fprintf(file,"\tFDE LSDA pointer encoding:");
      printEHPointerEncoding(file,cie->fdeLSDAPointerEncoding);
      fprintf(file,"\n");
    }
  }
  else
  {
    fprintf(file,"\taugmentation: \"\"");
  }
  fprintf(file,"\tInitial instructions:\n");
  for(int i=0;i<cie->numInitialInstructions;i++)
  {
    fprintf(file,"\t\t");
    printInstruction(file,cie->initialInstructions[i],0);
  }
  fprintf(file,"\tInitial register rules (computed from instructions)\n");
  printRules(file,cie->initialRules,"\t\t");
}

//The difference between this function and
//DwarfscriptCommand::printFDEInfo is that this function is geared
//towards displaying information to a human and the other is geared
//towards creating commands in a scripting language. While there may
//be some duplication not strictly necessary, neither function is
//difficult or complicated.
void printFDEInfo(FILE* file,FDE* fde,int num,ElfInfo* elf)
{
  CIE* cie=fde->cie;
  fprintf(file,"--------FDE #%i (at offset 0x%x----\n",num,fde->offset);
  if(elf && !elf->isPO)
  {
    fprintf(file,"\tfunction guess: %s\n",getFunctionNameAtPC(elf,fde->lowpc));
  }
  fprintf(file,"\tlowpc: 0x%x\n",fde->lowpc);
  fprintf(file,"\thighpc:0x%x\n",fde->highpc);
  int flags=flags=cie->augmentationFlags;
  if(flags & CAF_FDE_LSDA)
  {
    assert(fde->hasLSDAPointer);
    fprintf(file,"\tLSDA index: 0x%zx\n",fde->lsdaIdx);
  }
  fprintf(file,"  Instructions:\n");
  for(int i=0;i<fde->numInstructions;i++)
  {
    fprintf(file,"    ");
    printInstruction(file,fde->instructions[i],0);
  }
  fprintf(file,"    The table would be as follows\n");
  Dictionary* rulesDict=dictDuplicate(cie->initialRules,(DictDataCopy)duplicatePoRegRule);
  //use this to keep track of which instructions we've read so far so
  //we don't read the same ones over and over
  int numInstrsReadSoFar=0;

  //todo: figure out what we're actually using highpc for.
  //at one point I had highpc replaced by 1 in the below check but I'm
  //not sure what the purpose of that was
  for(int i=fde->lowpc;i<fde->highpc || 0==i;i++)
  {
    int instrsRead=0;
    int stopLocation=evaluateInstructionsToRules(fde->cie,fde->instructions+numInstrsReadSoFar,fde->numInstructions,rulesDict,fde->lowpc,i,&instrsRead);
    numInstrsReadSoFar+=instrsRead;
    if(stopLocation != i) 
    { 
      //dictDelete(rulesDict,NULL);
      continue;//Don't need to print this because will be dup
    }
    fprintf(file,"    ----Register Rules at text address 0x%x------\n",i);
    printRules(file,rulesDict,"      ");
  }
}

//elf is not required but can provide additional info if it is given
void printCallFrameInfo(FILE* file,CallFrameInfo* cfi,ElfInfo* elf)
{
  for(int i=0;i<cfi->numCIEs;i++)
  {
    printCIEInfo(file,cfi->cies+i);
  }
  for(int i=0;i<cfi->numFDEs;i++)
  {
    printFDEInfo(file,cfi->fdes+i,i+1,elf);//fdes have a 1-based numbering scheme
  }
}
