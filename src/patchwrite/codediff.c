/*
  File: codediff.c
  Author: James Oakley
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
  logprintf(ELL_INFO_V1,ELS_CODEDIFF,"testing whether subprograms for %s are identical\n",patcheeFunc->name);
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
  Elf_Scn* relocScn=NULL;
  //if -ffunction-sections is specified,
  //each function will have its own relocation section,
  //check this out first
  char buf[1024];
  snprintf(buf,1024,".rel.text.%s",patcheeFunc->name);
  relocScn=getSectionByName(oldBinary,buf);
  if(!relocScn)
  {
    relocScn=getSectionByName(oldBinary,".rel.text");
  }
  if(!relocScn)
  {
    death("%s does not have a .rel.text section\n");
  }
  List* oldRelocations=getRelocationItemsInRange(oldBinary,relocScn,patcheeFunc->lowpc,patcheeFunc->highpc);
  //if -ffunction-sections is specified,
  //each function will have its own relocation section,
  //check this out first
  snprintf(buf,1024,".rel.text.%s",patchedFunc->name);
  relocScn=getSectionByName(newBinary,buf);
  if(!relocScn)
  {
    relocScn=getSectionByName(newBinary,".rel.text");
  }
  if(!relocScn)
  {
    death("%s does not have a .rel.text section\n");
  }
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

      //check basic symbol stuff to make sure it's the same symbol
      if(strcmp(getString(oldBinary,symOld.st_name),getString(newBinary,symNew.st_name)) ||
         symOld.st_size != symNew.st_size ||
         symOld.st_info != symNew.st_info ||
         symOld.st_other != symNew.st_other)
      {
        //the symbols differ in some important regard
        //todo: the test against st_shndx isn't necessarily valid, sections
        //could have ben re-numbered between the two, although it's unlikely
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ (in more than value)\n",patcheeFunc->name);
        break;
      }

      
      //check sections for the symbols
      Elf_Scn* scnOld=elf_getscn(oldBinary->e,symOld.st_shndx);
      Elf_Scn* scnNew=elf_getscn(newBinary->e,symNew.st_shndx);
      GElf_Shdr shdrOld;
      GElf_Shdr shdrNew;
      gelf_getshdr(scnOld,&shdrOld);
      gelf_getshdr(scnNew,&shdrNew);
      char* scnNameOld=getScnHdrString(oldBinary,shdrOld.sh_name);
      char* scnNameNew=getScnHdrString(newBinary,shdrNew.sh_name);
      if(strcmp(scnNameOld,scnNameNew))
      {
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ in section (%s vs %s\n",patcheeFunc->name,scnNameOld,scnNameNew);
        break;
      }

      //check the addend
      //this may sometimes deal incorrectly with .rodata, since it's so opaque
      //but the chances of false negatives (which could lead to system instability)
      //are small. False positives will lead to more functions than necessary
      //being patched, which may make it harder to apply a patch
      if(getAddendForReloc(relocOld) != getAddendForReloc(relocNew))
      {
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, relocation addends differ\n",patcheeFunc->name);
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
    logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s did not change\n",patcheeFunc->name);
  }
  return retval;
}
