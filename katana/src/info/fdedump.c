/*
  File: fdedump.c
  Author: James Oakley
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

#ifdef legacy
void readDebugFrameForDump(Dwarf_Debug dbg)
{
  Dwarf_Fde *fdeData = NULL;
  Dwarf_Signed fdeElementCount = 0;
  Dwarf_Cie *cieData = NULL;
  Dwarf_Signed cieElementCount = 0;
  CIE* cie=zmalloc(sizeof(CIE));

  Dwarf_Error err;
  dwarf_get_fde_list(dbg, &cieData, &cieElementCount,
                                   &fdeData, &fdeElementCount, &err);

  
  //read the CIE
  Dwarf_Unsigned cieLength = 0;
  Dwarf_Small version = 0;
  char* augmenter = "";
  Dwarf_Ptr initInstr = 0;
  Dwarf_Unsigned initInstrLen = 0;
  assert(1==cieElementCount);
  //todo: all the casting here is hackish
  //should respect types more
  dwarf_get_cie_info(cieData[0],
                     &cieLength,
                     &version,
                     &augmenter,
                     &cie->codeAlign,
                     &cie->dataAlign,
                     &cie->returnAddrRuleNum,
                     &initInstr,
                     &initInstrLen, &err);
  cie->initialInstructions=parseFDEInstructions(dbg,initInstr,initInstrLen,cie->dataAlign,
                                                cie->codeAlign,&cie->numInitialInstructions);
  cie->initialRules=dictCreate(100);//todo: get rid of arbitrary constant 100
  evaluateInstructions(cie->initialInstructions,cie->numInitialInstructions,cie->initialRules,-1);
  //todo: bizarre bug, it keeps coming out as -1, which is wrong
  cie->codeAlign=1;
  
  fdes=zmalloc(fdeElementCount*sizeof(FDE));
  numFdes=fdeElementCount;
  for (int i = 0; i < fdeElementCount; i++)
  {
    Dwarf_Fde dfde=fdeData[i];
    Dwarf_Ptr instrs;
    Dwarf_Unsigned ilen;
    dwarf_get_fde_instr_bytes(dfde, &instrs, &ilen, &err);
    fdes[i].instructions=parseFDEInstructions(dbg,instrs,ilen,cie->dataAlign,cie->codeAlign,&fdes[i].numInstructions);
    Dwarf_Addr lowPC = 0;
    Dwarf_Unsigned funcLength = 0;
    Dwarf_Ptr fdeBytes = NULL;
    Dwarf_Unsigned fdeBytesLength = 0;
    Dwarf_Off cieOffset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fdeOffset = 0;
    dwarf_get_fde_range(dfde,
                        &lowPC, &funcLength,
                        &fdeBytes,
                        &fdeBytesLength,
                        &cieOffset, &cie_index,
                        &fdeOffset, &err);
    fdes[i].lowpc=lowPC;
    fdes[i].highpc=lowPC+funcLength;
    printf("fde has lowpc of %i and highpc of %i\n",fdes[i].lowpc,fdes[i].highpc);
    
  }
}
#endif


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
    evaluateInstructions(fde->instructions,fde->numInstructions,rulesDict,i-fde->lowpc);
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
  readDebugFrame(patch);
  printCIEInfo(patch->cie);
  for(int i=0;i<patch->numFdes;i++)
  {
    printFDEInfo(patch->cie,patch->fdes+i,i+1);//fdes have a 1-based numbering scheme
  }
}
