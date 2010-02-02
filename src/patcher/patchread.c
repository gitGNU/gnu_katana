/*
  File: patchread.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Read patch object files and apply them
*/

#include "elfparse.h"
#include "dwarftypes.h"
#include "hotpatch.h"
#include "target.h"
#include "fderead.h"
#include "dwarfvm.h"

void allocateMemoryForVarRelocation(VarInfo* var,TransformationInfo* trans)
{
  printf("allocating memory for relocating variabl %s\n",var->name);
  int length=var->type->length;
  var->newLocation=getFreeSpaceForTransformation(trans,length);
  byte* zeros=zmalloc(length);
  printf("zeroing out new memory\n");
  memcpyToTarget(var->newLocation,zeros,length);
  free(zeros);
  //todo: handle errors
}

void transformVarData(VarInfo* var,Map* fdeMap,ElfInfo* targetBin)
{
  printf("transforming var %s\n",var->name);
  FDE* transformerFDE=mapGet(fdeMap,&var->type->fde);
  if(!transformerFDE)
  {
    //todo: roll back atomically
    death("could not find transformer for variable\n");
  }
  patchDataWithFDE(var,transformerFDE,targetBin);
}

void relocateVar(VarInfo* var,ElfInfo* targetBin)
{
  //todo: how much do we need to do as long as we're patching code
  //at the moment not patching code
  performRelocations(targetBin,var);
}

void readAndApplyPatch(int pid,ElfInfo* targetBin,ElfInfo* patch)
{
  //go through variables in the patch file and apply their fixups
  //todo: we're assuming for now that symbols in the binary the patch was generated
  //with and in the target binary are going to have the same values
  //this isn't necessarily going to be the case
  DwarfInfo* diPatch=readDWARFTypes(patch);
  Map* fdeMap=readDebugFrame(patch);//get mapping between fde offsets and fde structures
  TransformationInfo trans;
  memset(&trans,0,sizeof(trans));
  for(List* cuLi=diPatch->compilationUnits;cuLi;cuLi=cuLi->next)
  {
    CompilationUnit* cu=cuLi->value;
    VarInfo** vars=(VarInfo**) dictValues(cu->tv->globalVars);
    for(int i=0;vars[i];i++)
    {
      GElf_Sym sym;
      //todo: return value checking
      int idx=getSymtabIdx(targetBin,vars[i]->name);
      getSymbol(targetBin,idx,&sym);
      vars[i]->oldLocation=sym.st_value;
      bool relocate=sym.st_size < vars[i]->type->length;
      printf("need to relocated is %i as st_size is %i and new size is %i\n",relocate,(int)sym.st_size,vars[i]->type->length);
      if(relocate)
      {
        allocateMemoryForVarRelocation(vars[i],&trans);
      }
      transformVarData(vars[i],fdeMap,targetBin);
      if(relocate)
      {
        relocateVar(vars[i],targetBin);
      }
    }
    free(vars);
  }
  
}
