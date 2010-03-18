/*
  File: dwinfo_dump.h
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: Dumps patch information from the dwarf section of the patch
*/

#include "dwarftypes.h"
#include <unistd.h>
#include <limits.h>

void printPatchDwarfInfo(ElfInfo* patch)
{
  if(!patch->dwarfInfo)
  {
    char cwd[PATH_MAX];
    getcwd(cwd,PATH_MAX);
    readDWARFTypes(patch,cwd);
  }
  DwarfInfo* di=patch->dwarfInfo;
  printf("This patch contains changes to the following compilation units:\n");
  for(List* li=di->compilationUnits;li;li=li->next)
  {
    CompilationUnit* cu=li->value;
    printf("  %s\n",cu->name);
  }
  
  printf("\nThe following functions have changed and will be patched:\n");
  for(List* li=di->compilationUnits;li;li=li->next)
  {
    CompilationUnit* cu=li->value;
    printf("  In %s:\n",cu->name);
    SubprogramInfo** subprograms=(SubprogramInfo**)dictValues(cu->subprograms);
    for(int i=0;subprograms[i];i++)
    {
      printf("    %s\n",subprograms[i]->name);
    }
    free(subprograms);
  }

  printf("\nThe following variables have changed and will be patched:\n");
  for(List* li=di->compilationUnits;li;li=li->next)
  {
    CompilationUnit* cu=li->value;
    printf("  In %s:\n",cu->name);
    VarInfo** vars=(VarInfo**)dictValues(cu->tv->globalVars);
    for(int i=0;vars[i];i++)
    {
      printf("    %s %s\n",vars[i]->type->name,vars[i]->name);
      //todo: should display an index
      printf("      transformed by FDE at offset %i\n",vars[i]->type->fde);
    }
    free(vars);
  }
}
