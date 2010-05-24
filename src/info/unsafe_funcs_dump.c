/*
  File: unsafe_funcs_dump.c
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: March 2010
  Description: Print a list of functions which are unsafe, patching
               will not occur while they have activation fames on the stack
*/

#include "elfparse.h"
#include "symbol.h"

void printPatchUnsafeFuncsInfo(ElfInfo* patch)
{
  Elf_Data* unsafeFunctionsData=getDataByERS(patch,ERS_UNSAFE_FUNCTIONS);
  if(!unsafeFunctionsData)
  {
    death("Patch object does not contain any unsafe functions data. This should not be\n");
  }
  printf("The following functions have been deemed 'unsafe' and patching will not occur while any of them have activation frames on the stack\n");
  size_t numUnsafeFunctions=unsafeFunctionsData->d_size/sizeof(idx_t);
  for(int i=0;i<numUnsafeFunctions;i++)
  {
    idx_t symIdx=((idx_t*)unsafeFunctionsData->d_buf)[i];
    GElf_Sym sym;
    getSymbol(patch,symIdx,&sym);
    char* symName=getString(patch,sym.st_name);
    printf("  %s\n",symName);
  }
        
}
