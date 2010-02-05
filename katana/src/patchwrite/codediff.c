/*
  File: codediff.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Determine if two functions are identical
*/

#include "elfparse.h"
#include "types.h"

bool areSubprogramsIdentical(SubprogramInfo* patcheeFunc,SubprogramInfo* patchedFunc,
                             ElfInfo* oldBinary,ElfInfo* newBinary)
{
  int len1=patcheeFunc->highpc-patcheeFunc->lowpc;
  int len2=patchedFunc->highpc-patchedFunc->lowpc;
  if(len1!=len2)
  {
    printf("subprogram for %s changed, one is larger than the other",patcheeFunc->name);
    return false;
  }
  //todo: this is not at all correct
  //need to diff the instructions, taking symbol relocations into account
  printf("returning that subprograms for %s are identical, but this may in fact not be the case\n",patcheeFunc->name);
  return true;
}
