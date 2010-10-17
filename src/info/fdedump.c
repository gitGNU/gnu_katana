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


Dwarf_Debug dbgForFDEDump;

void printCIEInfo(CIE* cie)
{
  printf("------CIE-----\n");
  printf("\tdata align: %i\n",(int)cie->dataAlign);
  printf("\tcode align: %u\n",(unsigned int)cie->codeAlign);
  printf("\trule for return address: %i\n",cie->returnAddrRuleNum);
  printf("\tInitial instructions:\n");
  for(int i=0;i<cie->numInitialInstructions;i++)
  {
    printf("\t\t");
    printInstruction(cie->initialInstructions[i]);
  }
  printf("\tInitial register rules (computed from instructions)\n");
  printRules(cie->initialRules,"\t\t");
}

void printFDEInfo(CIE* cie,FDE* fde,int num)
{
  printf("--------FDE #%i-----\n",num);
  //printf("\tfunction guess:%s\n",get_fde_proc_name(dbg,fde->lowpc));
  //printf("\tlowpc: 0x%x\n",fde->lowpc);
  //printf("\thighpc:0x%x\n",fde->highpc);
  printf("  Instructions:\n");
  for(int i=0;i<fde->numInstructions;i++)
  {
    printf("    ");
    printInstruction(fde->instructions[i]);
  }
  printf("    The table would be as follows\n");
  //todo: figure out what we're actually using highpc for.
  //at one point I had highpc replaced by 1 in the below check but I'm
  //not sure what the purpose of that was
  for(int i=fde->lowpc;i<fde->highpc || 0==i;i++)
  {
    Dictionary* rulesDict=dictDuplicate(cie->initialRules,NULL);
    evaluateInstructionsToRules(fde->instructions,fde->numInstructions,rulesDict,i-fde->lowpc);
    printf("    ----Register Rules at text address 0x%x------\n",i);
    printRules(rulesDict,"      ");
    dictDelete(rulesDict,NULL);
  }
}

void printPatchFDEInfo(ElfInfo* patch)
{
  /*Dwarf_Error err;
  if(DW_DLV_OK!=dwarf_elf_init(patch->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbgForFDEDump,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  readDebugFrameForDump(dbgForFDEDump);
  */
  printCIEInfo(patch->cie);
  for(int i=0;i<patch->numFdes;i++)
  {
    printFDEInfo(patch->cie,patch->fdes+i,i+1);//fdes have a 1-based numbering scheme
  }
}
