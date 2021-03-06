/*
  File: dwinfo_dump.h
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
