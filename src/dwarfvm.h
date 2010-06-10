/*
  File: dwarfvm.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: Virtual Machine for executing Dwarf DW_CFA instructions
*/

#ifndef dwarfvm_h
#define dwarfvm_h
#include "types.h"
#include "fderead.h"
void patchDataWithFDE(VarInfo* var,FDE* transformerFDE,ElfInfo* targetBin,ElfInfo* patch,ElfInfo* patchedBin);
//evaluates the given instructions and stores them in the output rules dictionary
//the initial condition of regarray IS taken into account
//execution continues until the end of the instructions or until the location is advanced
//past stopLocation. stopLocation should be relative to the start of the instructions (i.e. the instructions are considered to start at 0)
//if stopLocation is negative, it is ignored
void evaluateInstructionsToRules(RegInstruction* instrs,int numInstrs,Dictionary* rules,int stopLocation);

//stack length given in words
word_t evaluateDwarfExpression(byte* bytes,int len,word_t* startingStack,int stackLen);

void cleanupDwarfVM();
#endif
