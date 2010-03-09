/*
  File: safety.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: Determines when it is safe to patch and sets a breakpoint at the next safe point
*/

#include "register.h"
#include "fderead.h"
#include "dwarfvm.h"
#include "target.h"
#include <math.h>
#include "symbol.h"
#include <libunwind.h> //for determining the backtrace
#include <libunwind-ptrace.h>
#include "util/logging.h"

FDE* getFDEForPC(ElfInfo* elf,addr_t pc)
{
  assert(elf->fdes);
  //elf->fdes are sorted by lowpc, so we can do a binary search
  size_t low=0;
  size_t high=elf->numFdes;
  while(high-low>0)
  {
    size_t middle=low+(size_t)floor((float)(high-low)/2.0f);
    if(elf->fdes[middle].lowpc > pc)
    {
      high=middle-1;
    }
    else if(elf->fdes[middle].highpc < pc)
    {
      low=middle+1;
    }
    else
    {
      //we've found it!
      return &elf->fdes[middle];
    }
  }
  if(elf->fdes[low].lowpc <= pc && elf->fdes[low].highpc >= pc)
  {
    return &elf->fdes[low];
  }
  return NULL;
}

char* getFunctionNameAtPC(ElfInfo* elf,addr_t pc)
{
  idx_t symIdx=findSymbolContainingAddress(elf,pc,STT_FUNC);
  if(STN_UNDEF==symIdx)
  {
    return "?";
  }
  GElf_Sym sym;
  getSymbol(elf,symIdx,&sym);
  return getString(elf,sym.st_name);
}

//conversion between dwarf register numbers and x86 registers
void setRegValueFromDwarfRegNum(struct user_regs_struct* regs,int num,long int newValue)
{
  switch(num)
  {
  case 0:
    regs->eax=newValue;
    break;
  case 1:
    regs->ecx=newValue;
    break;
  case 2:
    regs->edx=newValue;
    break;
  case 3:
    regs->ebx=newValue;
    break;
  case 4:
    regs->esp=newValue;
    break;
  case 5:
    regs->ebp=newValue;
    break;
  case 6:
    regs->esi=newValue;
    break;
  case 7:
    regs->edi=newValue;
    break;
  case 8:
    regs->eip=newValue;
    break;
  default:
    fprintf(stderr,"unknown register number %i, cannot set reg val\n",num);
    death(NULL);
  }
}

//conversion between dwarf register numbers and x86 registers
long int getRegValueFromDwarfRegNum(struct user_regs_struct regs,int num)
{
  switch(num)
  {
  case 0:
    return regs.eax;
  case 1:
    return regs.ecx;
  case 2:
    return regs.edx;
  case 3:
    return regs.ebx;
  case 4:
    return regs.esp;
  case 5:
    return regs.ebp;
  case 6:
    return regs.esi;
  case 7:
    return regs.edi;
  case 8:
    return regs.eip;
  default:
    fprintf(stderr,"unknown register number %i, cannot get reg val\n",num);
    death(NULL);
  }
  return -1;
}

char* getX86RegNameFromDwarfRegNum(int num)
{
   switch(num)
  {
  case 0:
    return "eax";
  case 1:
    return "ecx";
  case 2:
    return "edx";
  case 3:
    return "ebx";
  case 4:
    return "esp";
  case 5:
    return "ebp";
  case 6:
    return "esi";
  case 7:
    return "edi";
  case 8:
    return "eip";
  default:
    fprintf(stderr,"unknown register number %i, cannot get reg name\n",num);
    death(NULL);
  }
  return "error";
}

#define NUM_X86_REGS 8

struct user_regs_struct restoreRegsFromRegisterRules(struct user_regs_struct currentRegs,
                                                     Dictionary* rulesDict)
{
  struct user_regs_struct regs=currentRegs;
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  PoRegRule* cfaRule=dictGet(rulesDict,strForReg(cfaReg));
  if(!cfaRule)
  {
    death("no way to compute cfa\n");
  }
  assert(ERRT_CFA==cfaRule->type);
  assert(ERT_BASIC==cfaRule->regRH.type);
  uint cfaAddr=getRegValueFromDwarfRegNum(currentRegs,cfaRule->regRH.u.index)+
    cfaRule->offset;
  for(int i=0;i<=NUM_X86_REGS;i++)
  {
    //printf("restoring reg %i\n",i);
    char buf[32];
    sprintf(buf,"r%i",i);
    PoRegRule* rule=dictGet(rulesDict,buf);
    if(!rule)
    {
      if(!strcmp("esp",getX86RegNameFromDwarfRegNum(i)))
      {
        //no rule to restore the stack pointer, assume it's the same as the cfa
        regs.esp=cfaAddr;
      }
      continue;
    }
    if(rule->type!=ERRT_UNDEF)
    {
      if(ERRT_REGISTER==rule->type)
      {
        //printf("type is register\n");
        assert(ERT_BASIC==rule->regRH.type);
        setRegValueFromDwarfRegNum(&regs,rule->regRH.u.index,
                                   getRegValueFromDwarfRegNum(currentRegs,
                                                              rule->regRH.u.index));
      }
      else if(ERRT_OFFSET==rule->type)
      {
        uint addr=(int)cfaAddr + rule->offset;
        long int value;
        memcpyFromTarget((byte*)&value,addr,sizeof(value));
        //printf("type is offset\n");
        setRegValueFromDwarfRegNum(&regs,i,value);
      }
      else
      {
        death("unknown rule type\n");
      }
    }
  }
  //printf("done restoring registers\n");
  return regs;
}

unw_addr_space_t unwindAddrSpace;
void* unwindUPTHandle;
unw_cursor_t unwindCursor;
void startLibUnwind(int pid)
{
  unwindAddrSpace=unw_create_addr_space(&_UPT_accessors,__LITTLE_ENDIAN);
  unwindUPTHandle=_UPT_create(pid);
  if(!unwindUPTHandle)
  {
    death("failed to initialize UPT in libunwind\n");
  }
  unw_init_remote(&unwindCursor,unwindAddrSpace,unwindUPTHandle);
}

void endLibUnwind()
{
  _UPT_destroy(unwindUPTHandle);
  unw_destroy_addr_space(unwindAddrSpace);
}

typedef struct
{
  addr_t pc;
  idx_t symIdx;
} ActivationFrame;

//returns list with data type ActivationFrame. This list will be a list
//of stack frames relevant to the target it will be ordered oldest
//(further up the stack) to newest (further down the stack). This
//ordering may be counter-intuitive, but makes sense because if we
//find a safety conflict in something up the stack, it invalidates
//everything further down the stack too
DList* findActivationFrames(ElfInfo* elf,int pid)
{
  DList* activationFramesHead=NULL;
  startLibUnwind(pid);
  unw_word_t ip;
  GElf_Shdr shdr;
  //todo: should support multiple text sections for applying
  //patches to already patched executables
  if(!gelf_getshdr(getSectionByERS(elf,ERS_TEXT),&shdr))
  {
    death("gelf_getshdr failed\n");
  }
  addr_t lowpc=shdr.sh_addr;
  addr_t highpc=lowpc+shdr.sh_size;
  
  while (unw_step(&unwindCursor) > 0)
  {
    unw_get_reg(&unwindCursor, UNW_REG_IP, &ip);
    if(lowpc<=ip && ip<=highpc)
    {
      //we found an activation frame that's in the text section for our program
      //(i.e. as opposed to in libc or something)
      //now we try to find its symbol.
      idx_t symIdx=findSymbolContainingAddress(elf,ip,STT_FUNC);
      if(STN_UNDEF!=symIdx)
      {
        logprintf(ELL_INFO_V1,ELS_SAFETY,"Found activation frame at 0x%x\n",ip);
        DList* li=zmalloc(sizeof(DList));
        li->value=zmalloc(sizeof(ActivationFrame));
        ((ActivationFrame*)li->value)->pc=ip;
        ((ActivationFrame*)li->value)->symIdx=symIdx;
        //push the frame (which will be further up the stack) onto the front
        //of the list. This gives us a list ordered from old to new
        //this is because rejecting an older frame will reject all newer ones
        dlistPush(&activationFramesHead,NULL,li);
      }
      //of if is STB_UNDEF, might be _start or something we don't care about
    }
  }
  endLibUnwind();
  return activationFramesHead;
}

//find a location in the target where nothing that's being patched is being used.
addr_t findSafeBreakpointForPatch(ElfInfo* targetBin,ElfInfo* patch,int pid)
{
  DList* activationFrames=findActivationFrames(targetBin,pid);
  Elf_Data* unsafeFunctionsData=getDataByERS(patch,ERS_UNSAFE_FUNCTIONS);
  size_t numUnsafeFunctions=unsafeFunctionsData->d_size/sizeof(idx_t);
  //have to go through and reindex them all
  idx_t* unsafeFunctions=zmalloc(numUnsafeFunctions*sizeof(idx_t));
  for(int i=0;i<numUnsafeFunctions;i++)
  {
    idx_t symIdxPatch=((idx_t*)unsafeFunctionsData->d_buf)[i];
    idx_t symIdxTarget=reindexSymbol(patch,targetBin,symIdxPatch,ESFF_VERSIONED_SECTIONS_OK);
    if(STN_UNDEF==symIdxTarget)
    {
      death("Failed to reindex symbol for unsafe function\n");
    }
    unsafeFunctions[i]=symIdxTarget;
  }
  addr_t deepestGoodPC=0;
  DList* li=activationFrames;
  for(;li;li=li->next)
  {
    ActivationFrame* frame=li->value;
    idx_t symIdx=frame->symIdx;
    bool unsafe=false;
    for(int i=0;i<numUnsafeFunctions;i++)
    {
      if(symIdx==unsafeFunctions[i])
      {
        unsafe=true;
        break;
      }
    }
    if(unsafe)
    {
      logprintf(ELL_INFO_V1,ELS_SAFETY,"Activation frame at 0x%x (%s) failed safety check\n",frame->pc,getFunctionNameAtPC(targetBin,frame->pc));
      if(li->prev)
      {
        li->prev->next=NULL;
      }
      deleteDList(li,free);
      li=NULL;
      break;
    }
    else
    {
      deepestGoodPC=((ActivationFrame*)li->value)->pc;
      logprintf(ELL_INFO_V1,ELS_SAFETY,"Activation frame at 0x%x (%s) passed safety check\n",deepestGoodPC,getFunctionNameAtPC(targetBin,frame->pc));
    }
  }
  if(!deepestGoodPC)
  {
    death("All functions with activation frames on the stack require patching. The application will never be in a patchable state!");
  }
  return deepestGoodPC;
}

void printBacktrace(ElfInfo* elf,int pid)
{
  startLibUnwind(pid);
  unw_word_t ip;
  GElf_Shdr shdr;
  //todo: should support multiple text sections for applying
  //patches to already patched executables
  if(!gelf_getshdr(getSectionByERS(elf,ERS_TEXT),&shdr))
  {
    death("gelf_getshdr failed\n");
  }
  addr_t lowpc=shdr.sh_addr;
  addr_t highpc=lowpc+shdr.sh_size;
  
  while (unw_step(&unwindCursor) > 0)
  {
    unw_get_reg(&unwindCursor, UNW_REG_IP, &ip);
    if(lowpc<=ip && ip<=highpc)
    {
      printf("0x%x: %s\n",ip,getFunctionNameAtPC(elf,ip));
    }
  }
  endLibUnwind();

  //below commented out is my initial version doing our own generation of the
  //backtrace rather than using libunwind. The problem with this is that we'd
  //have to read in .eh_frame from libc and do some messy stuff with that, so
  //I decided it would be easier just to use libunwind. --james
  /*
  struct user_regs_struct regs;
  getTargetRegs(&regs);
  printf("~~~~~Backtrace:~~~~~\n");
  for(int i=0;;i++)
  {
    FDE* fde=getFDEForPC(elf,regs.eip);
    if(!fde)
    {
      printf("Could not get FDE for pc 0x%x\n",(uint)regs.eip);
      printf("0x%x: ??\n",(uint)regs.eip);
      //do what we can with the CIE default rules
      //this is not really a good state of affairs because not always
      //enough. Really would have to find where libc was loaded into our
      //address space and get .eh_frame from libc. This would be cumbersome
      regs=restoreRegsFromRegisterRules(regs,elf->cie->initialRules);
      continue;
    }
    printf("%i. %s\n",i,getFunctionNameAtPC(elf,fde->lowpc));
    Dictionary* rulesDict=dictDuplicate(elf->cie->initialRules,NULL);
    evaluateInstructionsToRules(fde->instructions,fde->numInstructions,rulesDict,regs.eip);    
    printf("\t{eax=0x%x,ecx=0x%x,edx=0x%x,ebx=0x%x,esp=0x%x,\n\tebp=0x%x,esi=0x%x,edi=0x%x,eip=0x%x}\n",(uint)regs.eax,(uint)regs.ecx,(uint)regs.edx,(uint)regs.ebx,(uint)regs.esp,(uint)regs.ebp,(uint)regs.esi,(uint)regs.edi,(uint)regs.eip);
    printf("\tunwinding and applying register restore rules:\n");
    printRules(rulesDict,"\t");
    regs=restoreRegsFromRegisterRules(regs,rulesDict);
    dictDelete(rulesDict,NULL);
  }
  */
}

