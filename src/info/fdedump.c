/*
  File: fdedump.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
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
  //todo: figure out what we're actually using highpc for
  for(int i=fde->lowpc;i</*fde->highpc*/1 || 0==i;i++)
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
