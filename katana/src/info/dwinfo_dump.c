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
#include "fderead.h"

void printPatchDwarfInfo(ElfInfo* patch,Map* fdeMap)
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
    SubprogramInfo** subprograms=(SubprogramInfo**)dictValues(cu->subprograms);
    for(int i=0;subprograms[i];i++)
    {
      if(0==i)
      {
        //wait until here to print it in case there are none
        printf("  In %s:\n",cu->name);
      }
      printf("    %s\n",subprograms[i]->name);        
    }
    free(subprograms);
  }

  printf("\nThe following variables have changed and will be patched:\n");
  for(List* li=di->compilationUnits;li;li=li->next)
  {
    CompilationUnit* cu=li->value;
    VarInfo** vars=(VarInfo**)dictValues(cu->tv->globalVars);
    for(int i=0;vars[i];i++)
    {
      if(0==i)
      {
        //wait until here to print the name in case there are none
        printf("  In %s:\n",cu->name);
      }
      printf("    %s %s\n",vars[i]->type->name,vars[i]->name);
      //todo: should display an index
      FDE* transformerFDE=mapGet(fdeMap,&vars[i]->type->fde);
      if(transformerFDE)
      {
        printf("      transformed by FDE#%i\n",transformerFDE->idx);
      }
    }
    free(vars);
  }
}
