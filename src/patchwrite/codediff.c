/*
  File: codediff.c
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
  Description: Determine if two functions are identical
*/

#include "elfparse.h"
#include "types.h"
#include "relocation.h"
#include "util/list.h"
#include "symbol.h"
#include "util/logging.h"
#include <assert.h>

//compares two relocations based on r_offset
int cmpRelocs(void* a,void* b)
{
  RelocInfo* relocA=a;
  RelocInfo* relocB=b;
  return relocA->r_offset-relocB->r_offset;
}

bool areSubprogramsIdentical(SubprogramInfo* patcheeFunc,SubprogramInfo* patchedFunc,
                             ElfInfo* oldBinary,ElfInfo* newBinary)
{
  logprintf(ELL_INFO_V2,ELS_CODEDIFF,"testing whether subprograms for %s are identical\n",patcheeFunc->name);
  int len1=patcheeFunc->highpc-patcheeFunc->lowpc;
  int len2=patchedFunc->highpc-patchedFunc->lowpc;
  if(len1!=len2)
  {
    logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, one is larger than the other\n",patcheeFunc->name);
    return false;
  }

  //todo: add strict option where always return false if the compilation unit changed at all
  
  /*do a more thorough code diff. Go through byte-by-byte and apply the following rules
  1. Look for a relocation at the given address. If one exists, return
     false if they refer to different symbols, if they refer to the same
     symbol but with a different addend, or if the symbol refers to a
     variable of a changed type.
  2. If there is no relocation, return false if the bytes differ
  */
  Elf_Scn* relocScn=getRelocationSection(oldBinary,patcheeFunc->name);
  List* oldRelocations=getRelocationItemsInRange(oldBinary,relocScn,patcheeFunc->lowpc,patcheeFunc->highpc);

  relocScn=getRelocationSection(newBinary,patchedFunc->name);
  List* newRelocations=getRelocationItemsInRange(newBinary,relocScn,patchedFunc->lowpc,patchedFunc->highpc);

  if(listLength(oldRelocations) != listLength(newRelocations))
  {
    deleteList(newRelocations,free);
    deleteList(oldRelocations,free);
    logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, they contain different numbers of relocations\n",patcheeFunc->name);
    return false;
  }

  //sort both lists
  sortList(oldRelocations,&cmpRelocs);
  sortList(newRelocations,&cmpRelocs);
  //if -ffunction-sections is used, the function might have its own text section
  Elf_Scn* textScn=NULL;
  char buf[1024];
  snprintf(buf,1024,".text.%s",patcheeFunc->name);
  textScn=getSectionByName(oldBinary,buf);
  if(!textScn)
  {
    textScn=getSectionByERS(oldBinary,ERS_TEXT);
  }
  assert(textScn);
  byte* textOld=getDataAtAbs(textScn,patcheeFunc->lowpc,IN_MEM);
  //if -ffunction-sections is used, the function might have its own text section
  snprintf(buf,1024,".text.%s",patcheeFunc->name);
  textScn=getSectionByName(newBinary,buf);
  if(!textScn)
  {
    textScn=getSectionByERS(newBinary,ERS_TEXT);
  }
  assert(textScn);
  byte* textNew=getDataAtAbs(textScn,patchedFunc->lowpc,IN_MEM);
  List* oldLi=oldRelocations;
  List* newLi=newRelocations;
  bool retval=true;
  for(int i=0;i<len1;i++)
  {
    RelocInfo* relocOld=NULL;
    if(oldLi)
    {
      relocOld=oldLi->value;
    }
    RelocInfo* relocNew=NULL;
    if(newLi)
    {
      relocNew=newLi->value;
    }
    if(relocOld && relocNew &&
       (patcheeFunc->lowpc+i==relocOld->r_offset) &&
       (patchedFunc->lowpc+i==relocNew->r_offset))
    {
      GElf_Sym symOld;
      GElf_Sym symNew;
      getSymbol(oldBinary,relocOld->symIdx,&symOld);
      getSymbol(newBinary,relocNew->symIdx,&symNew);
      char* oldSymName=getString(oldBinary,symOld.st_name);
      char* newSymName=getString(newBinary,symNew.st_name);
      //check basic symbol stuff to make sure it's the same symbol
      byte oldType=ELF64_ST_TYPE(symOld.st_info);
      byte newType=ELF64_ST_TYPE(symNew.st_info);
      byte oldBind=ELF64_ST_BIND(symOld.st_info);
      byte newBind=ELF64_ST_BIND(symNew.st_info);
      if(strcmp(oldSymName,newSymName) ||
         oldType != newType ||
         oldBind != newBind ||
         symOld.st_other != symNew.st_other)
      {
        //the symbols differ in some important regard
        //todo: the test against st_shndx isn't necessarily valid, sections
        //could have ben re-numbered between the two, although it's unlikely
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols (for %s/%s) differ (in more than value). st_info is %u/%u, st_other is %u/%u\n",
                  patcheeFunc->name,oldSymName,newSymName,
                  (uint)symOld.st_info,(uint)symNew.st_info,
                  (uint)symOld.st_other,(uint)symNew.st_other);
        break;
      }
      if(symOld.st_size != symNew.st_size &&
         oldType!=STT_FUNC)
      {
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols (for %s) differ in size (and are not symbols for a function\n",patcheeFunc->name,oldSymName);
      }

      char* scnNameNew=NULL;
      if(symOld.st_shndx!=SHN_UNDEF && symOld.st_shndx!=SHN_COMMON &&
         symOld.st_shndx!=SHN_ABS && symNew.st_shndx!=SHN_UNDEF &&
         symNew.st_shndx!=SHN_COMMON && symNew.st_shndx!=SHN_ABS)
      {
        //check sections for the symbols
        Elf_Scn* scnOld=elf_getscn(oldBinary->e,symOld.st_shndx);
        assert(scnOld);
        Elf_Scn* scnNew=elf_getscn(newBinary->e,symNew.st_shndx);
        assert(scnNew);
        GElf_Shdr shdrOld;
        GElf_Shdr shdrNew;
        if(!gelf_getshdr(scnOld,&shdrOld))
        {
          death("gelf_getshdr failed\n");
        }
        if(!gelf_getshdr(scnNew,&shdrNew))
        {
          death("gelf_getshdr failed\n");
        }
        char* scnNameOld=getScnHdrString(oldBinary,shdrOld.sh_name);
        scnNameNew=getScnHdrString(newBinary,shdrNew.sh_name);
        if(strcmp(scnNameOld,scnNameNew))
        {
          retval=false;
          logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ in section (%s vs %s\n",patcheeFunc->name,scnNameOld,scnNameNew);
          break;
        }
      }
      else if((symOld.st_shndx==SHN_UNDEF || symOld.st_shndx==SHN_COMMON ||
               symOld.st_shndx==SHN_ABS) && symNew.st_shndx!=symOld.st_shndx)
      {
           logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ in section, special section types don't match\n",patcheeFunc->name);
          break;
      }
      else if((symNew.st_shndx==SHN_UNDEF || symNew.st_shndx==SHN_COMMON ||
               symNew.st_shndx==SHN_ABS) && symNew.st_shndx!=symOld.st_shndx)
      {
           logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ in section, special section types don't match\n",patcheeFunc->name);
          break;
      }

      //check the addend
      //this may sometimes deal incorrectly with .rodata, since it's so opaque
      //but the chances of false negatives (which could lead to system instability)
      //are small. False positives will lead to more functions than necessary
      //being patched, which may make it harder to apply a patch
      if(relocOld->r_addend != relocNew->r_addend)
      {
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, relocation addends differ for symbol '%s'' in section %s (ndx %i)\n",patcheeFunc->name,newSymName,scnNameNew,(int)symNew.st_shndx);
        break;
      }

      //todo: should we explicitly check the type of the variable the symbol refers
      //to and see if it's changed? Or should we assume that anything important is
      //taken care of by checking the addend?

      logprintf(ELL_INFO_V4,ELS_CODEDIFF,"Relocations at byte 0x%x determined to be the same\n",i);
      oldLi=oldLi->next;
      newLi=newLi->next;
      i+=sizeof(addr_t)-1;//since we compared on a whole address, not just the one byte
      continue;
    }
    else if(relocOld && patcheeFunc->lowpc+i==relocOld->r_offset) //somehow just one matches
    {
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, only old offset matches\n",patcheeFunc->name);
      retval=false;
      break;
    }
    else if(relocNew && patchedFunc->lowpc+i==relocNew->r_offset)
    {
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, only new offset matches\n",patcheeFunc->name);
      retval=false;
      break;
    }
    //ok, no relocation applies, just compare normally
    logprintf(ELL_INFO_V4,ELS_CODEDIFF,"Bytes at 0x%x are %x/%x\n",i,textOld[i],textNew[i]);

    if(textOld[i]!=textNew[i])
    {
      retval=false;
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, byte at 0x%x differs\n",patcheeFunc->name,(uint)i);
      break;
    }
  }
  
  deleteList(newRelocations,free);
  deleteList(oldRelocations,free);
  if(retval)
  {
    logprintf(ELL_INFO_V2,ELS_CODEDIFF,"subprogram for %s did not change\n",patcheeFunc->name);
  }
  return retval;
}
