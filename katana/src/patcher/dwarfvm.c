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
#include "target.h"
#include <assert.h>

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
        rule->reg=inst.arg1Reg;
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
      rule->reg2=inst.arg2Reg;
      break;
    case DW_CFA_def_cfa:
      cfaRule->type=ERRT_CFA;
      cfaRule->reg2=inst.arg1Reg;
      cfaRule->offset=inst.arg2;
      break;
    case DW_CFA_def_cfa_register:
      cfaRule->type=ERRT_CFA;
      cfaRule->reg2=inst.arg1Reg;
      break;
    case DW_CFA_def_cfa_offset:
      cfaRule->type=ERRT_CFA;
      cfaRule->offset=inst.arg1;
      break;
    case DW_CFA_nop:
      //do nothing, nothing changed
      break;
    default:
      death("unexpected instruction");
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

PatchData* makePatchData(PoRegRule* rule,SpecialRegsState* state)
{
  PatchData* result=zmalloc(sizeof(PatchData));
  byte* addrBytes;
  int numAddrBytes=resolveRegisterValue(&rule->reg,state,&addrBytes,true);
  if(sizeof(addr_t)!=numAddrBytes)
  {
    death("right hand register must always yield an address");
  }
  memcpy(&result->addr,addrBytes,sizeof(addr_t));
  switch(rule->type)
  {
  case ERRT_UNDEF:
    free(result);
    fprintf(stderr,"WARNING: undefined register, not doing anything. This is odd\n");
    return NULL;
    break;
  case ERRT_OFFSET:
    {
      addr_t addr=state->cfaValue+rule->offset;
      result->len=rule->reg.size?rule->reg.size:sizeof(word_t);
      result->data=zmalloc(result->len);
      memcpyFromTarget(result->data,addr,result->len);
    }
    break;
  case ERRT_REGISTER:
    result->len=resolveRegisterValue(&rule->reg2,state,&result->data,false);
    break;
  case ERRT_CFA:
    death("cfa should have been handled earlier\n");
    break;
  case ERRT_EXPR:
    death("expressions not handled yet. todo: implement them\n");
  default:
    death("unknown register rule type\n");
  }
  printf("patching 0x%x with the following bytes:\n{",(uint)result->addr);
  for(int i=0;i<result->len;i++)
  {
    printf("%u%s",(uint)result->data[i],i+1<result->len?",":"");
  }
  printf("}\n");
  return result;
}

//patchBin is the elf object we're mirroring all the changes
//we made to memory in so that it's possible to do successive patching
void patchDataWithFDE(VarInfo* var,FDE* fde,ElfInfo* oldBinaryElf)
{
  SpecialRegsState state;
  memset(&state,0,sizeof(state));
  state.currAddrOld=var->oldLocation;
  state.currAddrNew=var->newLocation;
  state.oldBinaryElf=oldBinaryElf;
  //we build up rules for each register from the FD_CFA instructions
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
    int nbytes=resolveRegisterValue(&cfaRule->reg2,&state,&cfaBytes,false);
    assert(sizeof(addr_t)==nbytes);
    memcpy(&addr,cfaBytes,sizeof(addr_t));
    state.cfaValue=addr+cfaRule->offset;
  }
  else
  {
    state.cfaValue=0;
  }
  List* liStart=NULL;
  List* liEnd=NULL;
  for(int i=0;rules[i];i++)
  {
    PatchData* pd=makePatchData(rules[i],&state);
    if(pd)
    {
      List* li=zmalloc(sizeof(List));
      li->value=pd;
      if(NULL==liStart)
      {
        liStart=li;
      }
      else
      {
        liEnd->next=li;
      }
      liEnd=li;
    }
  }
  //now that we've made all the patch data, apply it
  List* li=liStart;
  for(;li;li=li->next)
  {
    PatchData* pd=li->value;
    memcpyToTarget(pd->addr,pd->data,pd->len);
  }
  deleteList(liStart,free);
}
