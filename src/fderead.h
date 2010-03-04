/*
  File: fderead.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Read the FDE's from the section formatted as a .debug_frame section.
               These FDEs contain instructions used for patching the structure  
*/
#ifndef fderead_h
#define fderead_h
#include "elfparse.h"
#include "util/map.h"
#include "dwarf_instr.h"
//returns a Map between the numerical offset of an FDE
//(accessible via the DW_AT_MIPS_fde attribute of the relevant type)
//and the FDE structure
Map* readDebugFrame(ElfInfo* elf);


//not really using CIE at the moment
//do we need it?
typedef struct
{
  RegInstruction* initialInstructions;
  int numInitialInstructions;
  Dictionary* initialRules;
  Dwarf_Signed dataAlign;
  Dwarf_Unsigned codeAlign;
  Dwarf_Half returnAddrRuleNum;
} CIE;


typedef struct FDE
{
  CIE* cie;
  RegInstruction* instructions;
  int numInstructions;
  int memSize;//size of memory area the FDE describes. Used when fixing
           //up pointers to know how much mem to allocate
  int lowpc;
  int highpc;
  int offset;//offset from beginning of debug frame
} FDE;


//the returned memory should be freed
RegInstruction* parseFDEInstructions(Dwarf_Debug dbg,unsigned char* bytes,int len,
                                     int dataAlign,int codeAlign,int* numInstrs);



#endif
