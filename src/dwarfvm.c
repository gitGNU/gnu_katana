/*
  File: dwarfvm.c
  Author: James Oakley
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

//returns a list of PatchData objects
List* generatePatchesFromFDEAndState(FDE* fde,SpecialRegsState* state,ElfInfo* patch);

//evaluates the given instructions and stores them in the output regarray
//the initial condition of regarray IS taken into account
//execution continues until the end of the instructions or until the location is advanced
//past stopLocation. stopLocation should be relative to the start of the instructions (i.e. the instructions are considered to start at 0)
//if stopLocation is negative, it is ignored
void evaluateInstructions(RegInstruction* instrs,int numInstrs,Dictionary* rules,int stopLocation)
{
  PoRegRule* rule=NULL;
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  PoRegRule* cfaRule=dictGet(rules,strForReg(cfaReg));
  int loc=0;
  for(int i=0;i<numInstrs;i++)
  {
    RegInstruction inst=instrs[i];
    if(ERT_NONE==inst.arg1Reg.type)
    {
      death("cannot evaluate register with type ERT_NONE\n");
      break;
    }
    else
    {
      rule=dictGet(rules,strForReg(inst.arg1Reg));
      if(!rule)
      {
        rule=zmalloc(sizeof(PoRegRule));
        rule->regLH=inst.arg1Reg;
        dictInsert(rules,strForReg(inst.arg1Reg),rule);
      }
    }
    //printf("evaluating instruction of type 0x%x\n",(uint)inst.type);
    switch(inst.type)
    {
    case DW_CFA_set_loc:
      loc=inst.arg1;
      if(stopLocation >= 0 && loc>stopLocation)
      {
        return;
      }
      break;
    case DW_CFA_advance_loc:
    case DW_CFA_advance_loc1:
    case DW_CFA_advance_loc2:
      loc+=inst.arg1;
      if(stopLocation >= 0 && loc>stopLocation)
      {
        return;
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
      cfaRule->type=ERRT_CFA;
      cfaRule->regRH=inst.arg1Reg;
      cfaRule->offset=inst.arg2;
      break;
    case DW_CFA_def_cfa_register:
      cfaRule->type=ERRT_CFA;
      cfaRule->regRH=inst.arg1Reg;
      break;
    case DW_CFA_def_cfa_offset:
      cfaRule->type=ERRT_CFA;
      cfaRule->offset=inst.arg1;
      break;
    case DW_CFA_KATANA_do_fixups:
      rule->type=ERRT_RECURSE_FIXUP;
      rule->regRH=inst.arg2Reg;
      rule->index=inst.arg3;
    case DW_CFA_nop:
      //do nothing, nothing changed
      break;
    default:
      death("unexpected instruction in evaluateInstructions");
    }
  }
}

//structure for holding one contiguous chunk of data
//to be put into the target process
typedef struct
{
  byte* data;//data to be poked into the target
  uint len;//how much data
  addr_t addr;//address to copy the data to
} PatchData;

//returns a list of PatchData objects
//this list generally only has one item unless a recurse rule
//was encountered
List* makePatchData(PoRegRule* rule,SpecialRegsState* state,ElfInfo* patch)
{
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

  if(ERRT_OFFSET==rule->type || ERRT_REGISTER==rule->type || ERRT_EXPR==rule->type)
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
      //pass true because don't want to dereference it
      int size=resolveRegisterValue(&rule->regRH,state,&rhAddrBytes,ERRF_NONE);
      assert(size==sizeof(addr_t));
      memcpy(&tmpState.currAddrOld,rhAddrBytes,sizeof(addr_t));
      //fde indices seem to be 1-based and we store them zero-based
      head=generatePatchesFromFDEAndState(&patch->fdes[rule->index-1],&tmpState,patch);
    }
    break;
  default:
    death("unknown register rule type\n");
  }
  #ifdef DEBUG
  if(result)
  {
    logprintf(ELL_INFO_V2,ELS_HOTPATCH_DATA,"patching 0x%x with the following bytes:\n{",(uint)result->addr);
    for(int i=0;i<result->len;i++)
    {
      logprintf(ELL_INFO_V2,ELS_HOTPATCH_DATA,"%u%s",(uint)result->data[i],i+1<result->len?",":"");
    }
    logprintf(ELL_INFO_V2,ELS_HOTPATCH_DATA,"}\n");
  }
  #endif
  return head;
}

//returns a list of PatchData objects
List* generatePatchesFromFDEAndState(FDE* fde,SpecialRegsState* state,ElfInfo* patch)
{
  //we build up rules for each register from the DW_CFA instructions
  Dictionary* rulesDict=dictCreate(100);//todo: get rid of arbitrary constant 100
  //todo: versioning?
  evaluateInstructions(fde->instructions,fde->numInstructions,rulesDict,fde->highpc);
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
  PoRegRule* cfaRule=dictGet(rulesDict,strForReg(cfaReg));
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
    List* patchList=makePatchData(rules[i],state,patch);
    liStart=concatLists(liStart,liEnd,patchList,NULL,&liEnd);
  }
  return liStart;
}

//patchBin is the elf object we're mirroring all the changes
//we made to memory in so that it's possible to do successive patching
void patchDataWithFDE(VarInfo* var,FDE* fde,ElfInfo* oldBinaryElf,ElfInfo* patch)
{
  SpecialRegsState state;
  memset(&state,0,sizeof(state));
  state.currAddrOld=var->oldLocation;
  state.currAddrNew=var->newLocation;
  state.oldBinaryElf=oldBinaryElf;

  List* patchesList=generatePatchesFromFDEAndState(fde,&state,patch);
  //now that we've made all the patch data, apply it
  List* li=patchesList;
  for(;li;li=li->next)
  {
    PatchData* pd=li->value;
    memcpyToTarget(pd->addr,pd->data,pd->len);
  }
  //todo: not right, need a free patch data function
  deleteList(patchesList,free);
}
