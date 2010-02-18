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
  List* oldRelocations=getRelocationItemsInRange(oldBinary,getSectionByName(oldBinary,".rel.text"),patcheeFunc->lowpc,patcheeFunc->highpc);
  List* newRelocations=getRelocationItemsInRange(newBinary,getSectionByName(newBinary,".rel.text"),patchedFunc->lowpc,patchedFunc->highpc);

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
  byte* textOld=getTextDataAtAbs(oldBinary,patcheeFunc->lowpc,IN_MEM);
  byte* textNew=getTextDataAtAbs(newBinary,patchedFunc->lowpc,IN_MEM);
  List* oldLi=oldRelocations;
  List* newLi=newRelocations;
  bool retval=true;
  for(int i=0;i<len1;i++)
  {
    RelocInfo* relocOld=oldLi->value;
    RelocInfo* relocNew=newLi->value;
    if((patcheeFunc->lowpc+i==relocOld->r_offset) &&
       (patchedFunc->lowpc+i==relocNew->r_offset))
    {
      GElf_Sym symOld;
      GElf_Sym symNew;
      if(!getSymbol(oldBinary,relocOld->symIdx,&symOld) ||
         !getSymbol(newBinary,relocNew->symIdx,&symNew))
      {
        death("Could not get symbols for relocations\n");
      }

      //check basic symbol stuff to make sure it's the same symbol
      if(strcmp(getString(oldBinary,symOld.st_name),getString(newBinary,symNew.st_name)) ||
         symOld.st_size != symNew.st_size ||
         symOld.st_info != symNew.st_info ||
         symOld.st_other != symNew.st_other ||
         symOld.st_shndx != symNew.st_shndx)
      {
        //the symbols differ in some important regard
        //todo: the test against st_shndx isn't necessarily valid, sections
        //could have ben re-numbered between the two, although it's unlikely
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, symbols differ (in more than value)\n",patcheeFunc->name);
        break;
      }

      //check the addend
      if(getAddendForReloc(relocOld) != getAddendForReloc(relocNew))
      {
        retval=false;
        logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, relocation addends differ\n",patcheeFunc->name);
        break;
      }

      //todo: should we explicitly check the type of the variable the symbol refers
      //to and see if it's changed? Or should we assume that anything important is
      //taken care of by checking the addend?
      
      i+=sizeof(addr_t)-1;//since we compared on a whole address, not just the one byte
    }
    else if(patcheeFunc->lowpc+i==relocOld->r_offset) //somehow just one matches
    {
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, only old offset matches\n",patcheeFunc->name);
      retval=false;
      break;
    }
    else if(patchedFunc->lowpc+i==relocNew->r_offset)
    {
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, only new offset matches\n",patcheeFunc->name);
      retval=false;
      break;
    }
    //ok, no relocation applies, just compare normally
    if(textOld[i]!=textNew[i])
    {
      retval=false;
      logprintf(ELL_INFO_V1,ELS_CODEDIFF,"subprogram for %s changed, byte at %i differs\n",patcheeFunc->name,i);
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
