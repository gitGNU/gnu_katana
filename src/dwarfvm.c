/*
  File: dwarfvm.c
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
  Description: Virtual Machine for executing Dwarf DW_CFA instructions
*/

#include "dwarfvm.h"
#include "register.h"
#include <dwarf.h>
#include "util/dictionary.h"
#include "patcher/target.h"
#include <assert.h>
#include "util/logging.h"
#include "symbol.h"
#include "util/map.h"
#include "patcher/hotpatch.h"

//returns a list of PatchData objects
List* generatePatchesFromFDEAndState(FDE* fde,SpecialRegsState* state,ElfInfo* patch,ElfInfo* patchedBin);

//keys are old addresses of data objects (variables). Values are the new addresses
Map* dataMoved=NULL;

//evaluates the given instructions and stores them in the output rules dictionary
//the initial condition of regarray IS taken into account
//execution continues until the end of the instructions or until the location is advanced
//past stopLocation. stopLocation should be relative to the start of the instructions (i.e. the instructions are considered to start at 0)
//if stopLocation is negative, it is ignored
//if stopLocation is negative, it is ignored (evaluation continues to the end of the instructions)
//returns the location stopped at
int evaluateInstructionsToRules(RegInstruction* instrs,int numInstrs,Dictionary* rules,int startLocation, int stopLocation)
{
  PoRegRule* rule=NULL;
  int loc=startLocation;
  for(int i=0;i<numInstrs;i++)
  {
    RegInstruction inst=instrs[i];
    if(inst.type==DW_CFA_set_loc)
    {
      loc=inst.arg1;
      if(stopLocation >= 0 && loc>stopLocation)
      {
        return loc;
      }
      continue;
    }
    else if(inst.type==DW_CFA_advance_loc ||
       inst.type==DW_CFA_advance_loc1 ||
       inst.type==DW_CFA_advance_loc2)
    {
      loc+=inst.arg1;
      if(stopLocation >=0 && loc>stopLocation)
      {
        return loc;
      }
      continue;
    }

/*     if(ERT_NONE==inst.arg1Reg.type) */
/*     { */
/*       death("cannot evaluate register with type ERT_NONE\n"); */
/*     } */
    PoReg reg;
    char* str=NULL;
    if(DW_CFA_def_cfa==inst.type ||
       DW_CFA_def_cfa_register==inst.type ||
       DW_CFA_def_cfa_offset==inst.type)
    {
      memset(&reg,0,sizeof(reg));
      reg.type=ERT_CFA;
    }
    else
    {
      reg=inst.arg1Reg;
    }
    str=strForReg(reg);
              
    rule=dictGet(rules,str);
    if(!rule)
    {
      rule=zmalloc(sizeof(PoRegRule));
      rule->regLH=reg;
      dictInsert(rules,str,rule);
        
    }
    free(str);
    //printf("evaluating instruction of type 0x%x\n",(uint)inst.type);
    switch(inst.type)
    {
    case DW_CFA_set_loc:
      loc=inst.arg1;
      if(stopLocation >= 0 && loc>stopLocation)
      {
        return loc;
      }
      break;
    case DW_CFA_advance_loc:
    case DW_CFA_advance_loc1:
    case DW_CFA_advance_loc2:
      loc+=inst.arg1;
      if(stopLocation >= 0 && loc>stopLocation)
      {
        return loc;
      }
      break;
    case DW_CFA_offset:
      rule->type=ERRT_OFFSET;
      rule->offset=inst.arg2;
      break;
    case DW_CFA_register:
      rule->type=ERRT_REGISTER;
      rule->regRH=inst.arg2Reg;
      break;
    case DW_CFA_def_cfa:
      rule->type=ERRT_CFA;
      rule->regRH=inst.arg1Reg;
      rule->offset=inst.arg2;
      break;
    case DW_CFA_def_cfa_register:
      rule->type=ERRT_CFA;
      rule->regRH=inst.arg1Reg;
      break;
    case DW_CFA_def_cfa_offset:
      rule->type=ERRT_CFA;
      rule->offset=inst.arg1;
      break;
    case DW_CFA_KATANA_fixups:
      rule->type=ERRT_RECURSE_FIXUP;
      rule->regRH=inst.arg2Reg;
      rule->index=inst.arg3;
      break;
    case DW_CFA_KATANA_fixups_pointer:
      rule->type=ERRT_RECURSE_FIXUP_POINTER;
      rule->regRH=inst.arg2Reg;
      rule->index=inst.arg3;
      break;
    case DW_CFA_nop:
      //do nothing, nothing changed
      break;
    default:
      death("unexpected instruction in evaluateInstructions");
    }
  }
  return loc;
}

//structure for holding one contiguous chunk of data
//to be put into the target process
//todo: there is a problem with our current scheme. We do not do patches immediately
//but batch them together for application later. This is the reason this data structure exists
//that in itself is not a bad thing, but it can happen that what's being read and what's been written
//get out of step in a way that perhaps they shouldn't be. Perhaps PatchData could hold another address to read from the
//target instead of data stored in memory? This would cut down on data stored in memory and it could help keep things in sync
typedef struct
{
  byte* data;//data to be poked into the target
  uint len;//how much data
  addr_t addr;//address to copy the data to
} PatchData;

void freePatchData(PatchData* pd)
{
  if(pd->data)
  {
    free(pd->data);
  }
  free(pd);
}

//returns a list of PatchData objects
//this list generally only has one item unless a recurse rule
//was encountered
List* makePatchData(PoRegRule* rule,SpecialRegsState* state,ElfInfo* patch,ElfInfo* patchedBin)
{
  if(!dataMoved)
  {
    dataMoved=size_tMapCreate(100);//todo: get rid of arbitrary constant 100
  }
  List* head=NULL;
  PatchData* result=NULL;
  byte* addrBytes;
  int numAddrBytes=resolveRegisterValue(&rule->regLH,state,&addrBytes,ERRF_ASSIGN);
  addr_t resultAddr;
  if(sizeof(addr_t)!=numAddrBytes)
  {
    death("left hand register must always yield an address");
  }
  memcpy(&resultAddr,addrBytes,sizeof(addr_t));
  free(addrBytes);

  if(ERRT_OFFSET==rule->type || ERRT_REGISTER==rule->type || ERRT_EXPR==rule->type ||
     ERRT_RECURSE_FIXUP_POINTER==rule->type)
  {
    head=zmalloc(sizeof(List));
    result=zmalloc(sizeof(PatchData));
    head->value=result;
    result->addr=resultAddr;
  }
  
  switch(rule->type)
  {
  case ERRT_UNDEF:
    free(result);
    free(head);
    fprintf(stderr,"WARNING: undefined register, not doing anything. This is odd\n");
    return NULL;
    break;
  case ERRT_OFFSET:
    {
      addr_t addr=state->cfaValue+rule->offset;
      result->len=rule->regLH.size?rule->regLH.size:sizeof(word_t);
      result->data=zmalloc(result->len);
      memcpyFromTarget(result->data,addr,result->len);
    }
    break;
  case ERRT_REGISTER:
    result->len=resolveRegisterValue(&rule->regRH,state,&result->data,ERRF_DEREFERENCE);
    break;
  case ERRT_CFA:
    death("cfa should have been handled earlier\n");
    break;
  case ERRT_EXPR:
    death("expressions not handled yet. todo: implement them\n");
    break;
  case ERRT_RECURSE_FIXUP:
    {
      SpecialRegsState tmpState=*state;
      tmpState.currAddrNew=resultAddr;
      byte* rhAddrBytes=NULL;
      int size=resolveRegisterValue(&rule->regRH,state,&rhAddrBytes,ERRF_NONE);
      assert(size==sizeof(addr_t));
      memcpy(&tmpState.currAddrOld,rhAddrBytes,sizeof(addr_t));
      free(rhAddrBytes);
      //fde indices seem to be 1-based and we store them zero-based
      head=generatePatchesFromFDEAndState(&patch->fdes[rule->index-1],&tmpState,patch,patchedBin);
    }
    break;
  case ERRT_RECURSE_FIXUP_POINTER:
    {
      //there are some special challenges when fixing up a pointer
      //0. make sure it isn't a NULL pointer
      //1. we have to make sure we have't already fixed up at that location yet
      //2. if the location corresponds to a symbol then it might
      //   perhaps be supposed to be relocated to a .data.new section
      //   or something like that. On the other hand, if it doesn't,
      //   then we can just allocate memory for it anywhere we like
      //   before we actually call generate PatchesFromFDEAndState

      //check for null pointer first,
      //smth will always get written to patch data,
      //whether it's the new memory address or
      //whether it's left NULL if it's a NULL pointer
      result->data=zmalloc(sizeof(addr_t));
      result->len=sizeof(addr_t);
      SpecialRegsState tmpState=*state;
      byte* rhAddrBytes=NULL;
      //remember that ERRF_DEREFERENCE means only that we look up the value
      //at the mem location indicated by the register, not that we then dereference
      //that mem location
      int size=resolveRegisterValue(&rule->regRH,state,&rhAddrBytes,ERRF_DEREFERENCE);
      assert(size==sizeof(addr_t));
      memcpy(&tmpState.currAddrOld,rhAddrBytes,sizeof(addr_t));
      free(rhAddrBytes);
      if(!tmpState.currAddrOld)
      {
        //NULL pointer, can't recurse on it. Just make sure 0 gets copied to the new location
        //we've already created the patch data as zero though, so we're all set
        break;
      }
      

      addr_t* existingDataMove=NULL;
      if((existingDataMove=mapGet(dataMoved,&tmpState.currAddrOld)))
      {
        logprintf(ELL_INFO_V2,ELS_DWARF_FRAME,"Found existing data move for addr 0x%x at 0x%x\n",tmpState.currAddrOld,existingDataMove);
        //we've already fixed up the location,
        //just have to set the pointer to point where we want it to
        memcpy(result->data,existingDataMove,sizeof(addr_t));
        result->len=sizeof(addr_t);
        break;
      }

      addr_t pointedObjectNewLocation=0;
      
      //now we have to see if the location corresponds to a symbol
      //that may be being relocated to a .data.new section or something
      //or the symbol itself may not even move
      idx_t symIdxOld=findSymbolContainingAddress(state->oldBinaryElf,tmpState.currAddrOld,STT_OBJECT,SHN_UNDEF);
      if(symIdxOld!=STN_UNDEF)
      {
        //ok, so it was in a symbol in the executing binary, Now we have to find
        //out where it might have been moved to. This will be the new location of
        //the .data.new section plus the value of the symbol in the patch
        idx_t symIdxPatch=reindexSymbol(state->oldBinaryElf,patch,symIdxOld,ESFF_FUZZY_MATCHING_OK|ESFF_BSS_MATCH_DATA_OK);
        if(STN_UNDEF==symIdxPatch)
        {
          death("need to fix up a pointer that is supposedly part of a variable (rather than arbitrary stuff on the heap) but the patch doesn't seem to contain this variable\n");
        }

        GElf_Sym sym;
        getSymbol(patch,symIdxPatch,&sym);
        assert(sym.st_shndx==elf_ndxscn(getSectionByERS(patch,ERS_DATA)));
        Elf_Scn* scn=getSectionByName(patchedBin,".data.new");
        assert(scn);
        GElf_Shdr shdr;
        if(!gelf_getshdr(scn,&shdr))
        {death("gelf_getshdr failed\n");}
        pointedObjectNewLocation=shdr.sh_addr+sym.st_value;
        logprintf(ELL_INFO_V2,ELS_DWARF_FRAME,"Found a symbol in target corresponding to this symbol(%s), so we're making our new location be 0x%zx as specified by the patch\n",getString(patch,sym.st_name),pointedObjectNewLocation);
      }
      else
      {
        
        //no variable associated with this, it's just some random data
        //on the heap. So we just allocate some random space for it
        //the problem now is, we have to know how much space to allocate
        //for this purpose we can use the address_range field
        //of the fde we're targeting

        //todo: deal with freeing the original memory
        //      and issues if the original var is
        //      part of a larger block, not on its own
        //      (this is very hard to get right because we're
        //      lacking important information)
        
        //pointedObjectNewLocation=getFreeSpaceInTarget(patch->fdes[rule->index-1].memSize);
        pointedObjectNewLocation=mallocTarget(patch->fdes[rule->index-1].memSize);
        logprintf(ELL_INFO_V2,ELS_DWARF_FRAME,"No symbol associated with object at address 0x%zx we have to relocate that we have a pointer to. Mallocced new memory at 0x%zx\n",tmpState.currAddrOld,pointedObjectNewLocation);
      }

      addr_t* value=zmalloc(sizeof(addr_t));
      *value=pointedObjectNewLocation;
      size_t* key=zmalloc(sizeof(size_t));
      memcpy(key,&tmpState.currAddrOld,sizeof(size_t));
      mapInsert(dataMoved,key,value);
      
      
      tmpState.currAddrNew=pointedObjectNewLocation;
      memcpy(result->data,&pointedObjectNewLocation,sizeof(addr_t));
      
      //fde indices seem to be 1-based and we store them zero-based
      head->next=generatePatchesFromFDEAndState(&patch->fdes[rule->index-1],&tmpState,patch,patchedBin);
    }
    break;
  default:
    death("unknown register rule type\n");
  }
  #ifdef DEBUG
  if(result)
  {
    logprintf(ELL_INFO_V2,ELS_HOTPATCH,"patching 0x%x with the following bytes:\n{",(uint)result->addr);
    for(int i=0;i<result->len;i++)
    {
      logprintf(ELL_INFO_V2,ELS_HOTPATCH,"%x%s",(uint)result->data[i],i+1<result->len?",":"");
    }
    logprintf(ELL_INFO_V2,ELS_HOTPATCH,"}\n");
  }
  #endif
  return head;
}

//returns a list of PatchData objects
List* generatePatchesFromFDEAndState(FDE* fde,SpecialRegsState* state,ElfInfo* patch,ElfInfo* patchedBin)
{
  //we build up rules for each register from the DW_CFA instructions
  Dictionary* rulesDict=dictCreate(100);//todo: get rid of arbitrary constant 100
  //todo: versioning?
  evaluateInstructionsToRules(fde->instructions,fde->numInstructions,rulesDict,fde->lowpc,fde->highpc);
  PoRegRule** rules=(PoRegRule**)dictValues(rulesDict);
  //we gather all of the the patch data together first before actually poking the target
  //because everything is supposed to be applied in parallel, as a table, and
  //it is possible that some writes would affect some reads, so we must
  //do all the reads first

  //we read the CFA rule first, if it exists, however because the CFA may be needed
  //to determine the value of other registers
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  char* str=strForReg(cfaReg);
  PoRegRule* cfaRule=dictGet(rulesDict,str);
  free(str);
  if(cfaRule)
  {
    addr_t addr;
    byte* cfaBytes;
    int nbytes=resolveRegisterValue(&cfaRule->regRH,state,&cfaBytes,ERRF_NONE);
    assert(sizeof(addr_t)==nbytes);
    memcpy(&addr,cfaBytes,sizeof(addr_t));
    state->cfaValue=addr+cfaRule->offset;
  }
  else
  {
    state->cfaValue=0;
  }
  List* liStart=NULL;
  List* liEnd=NULL;
  for(int i=0;rules[i];i++)
  {
    List* patchList=makePatchData(rules[i],state,patch,patchedBin);
    liStart=concatLists(liStart,liEnd,patchList,NULL,&liEnd);
  }
  free(rules);
  dictDelete(rulesDict,free);
  return liStart;
}

//patchBin is the elf object we're mirroring all the changes
//we made to memory in so that it's possible to do successive patching
void patchDataWithFDE(VarInfo* var,FDE* fde,ElfInfo* oldBinaryElf,ElfInfo* patch,ElfInfo* patchedBin)
{

  SpecialRegsState state;
  memset(&state,0,sizeof(state));
  state.currAddrOld=var->oldLocation;
  state.currAddrNew=var->newLocation;
  state.oldBinaryElf=oldBinaryElf;

  List* patchesList=generatePatchesFromFDEAndState(fde,&state,patch,patchedBin);
  //now that we've made all the patch data, apply it
  List* li=patchesList;
  for(;li;li=li->next)
  {
    PatchData* pd=li->value;
    memcpyToTarget(pd->addr,pd->data,pd->len);
  }
  deleteList(patchesList,(FreeFunc)freePatchData);
}

//helper function for evaluationDwarfExpression
word_t popDWVMStack(word_t* stack,int* stackLen)
{
  if(0==*stackLen)
  {
    death("Attempt to pop empty Dwarf stack\n");
  }
  (*stackLen)--;
  return stack[*stackLen];
}

//helper function for evaluationDwarfExpression
void pushDWVMStack(word_t** stack,int* stackLen,int* stackAlloced,word_t value)
{
  if(*stackLen+1==*stackAlloced)
  {
    *stackAlloced=max(2*(*stackAlloced),2);
    *stack=realloc(stack,*stackAlloced);
    MALLOC_CHECK(*stack);
  }
  (*stack)[*stackLen]=value;
  (*stackLen)++;
}

//stack length given in words
word_t evaluateDwarfExpression(byte* bytes,int len,word_t* startingStack,int stackLen)
{
  int stackAlloced=stackLen+10;
  word_t* stack=zmalloc(stackAlloced*sizeof(word_t));//alloc more as needed
  if(stackLen)
  {
    memcpy(stack,startingStack,stackLen);
  }
  
  for(int cnt=0;cnt<len;)
  {
    byte op=bytes[cnt++];
    switch(op)
    {
    case DW_OP_plus_uconst:
      {
        word_t top=popDWVMStack(stack,&stackLen);
        usint bytesRead;
        word_t uconst=leb128ToUWord(bytes+cnt,&bytesRead);
        cnt+=bytesRead;
        pushDWVMStack(&stack,&stackLen,&stackAlloced,top+uconst);
      }
      break;
    default:
      death("Dwarf expression operand %i not supported yet\n",(int)op);
    }
  }
  return popDWVMStack(stack,&stackLen);
}

void cleanupDwarfVM()
{
  if(dataMoved)
  {
    mapDelete(dataMoved,free,free);
  }
}
