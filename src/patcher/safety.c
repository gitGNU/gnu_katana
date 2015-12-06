/*
  File: safety.c
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
#include "constants.h"
#include <unistd.h>
#include <sys/wait.h>
#include "safety.h"
#include "katana_config.h"
#include "elfutil.h"

static const int MAX_WAIT_SECONDS_BEFORE_TRY_CURRENT_FRAME = 2;

FDE* getFDEForPC(ElfInfo* elf,addr_t pc)
{
  assert(elf->callFrameInfo.fdes);
  //elf->callFrameInfo.fdes are sorted by lowpc, so we can do a binary search
  size_t low=0;
  size_t high=elf->callFrameInfo.numFDEs;
  while(high-low>0)
  {
    size_t middle=low+(size_t)floor((float)(high-low)/2.0f);
    if(elf->callFrameInfo.fdes[middle].lowpc > pc)
    {
      high=middle-1;
    }
    else if(elf->callFrameInfo.fdes[middle].highpc < pc)
    {
      low=middle+1;
    }
    else
    {
      //we've found it!
      return &elf->callFrameInfo.fdes[middle];
    }
  }
  if(elf->callFrameInfo.fdes[low].lowpc <= pc && elf->callFrameInfo.fdes[low].highpc >= pc)
  {
    return &elf->callFrameInfo.fdes[low];
  }
  return NULL;
}


//this function and the one below it are coded badly. This can be done
//more efficiently

//conversion between dwarf register numbers and x86 registers
void setRegValueFromDwarfRegNum(struct user_regs_struct* regs,int num,long int newValue)
{
  #ifdef KATANA_X86_ARCH
  switch(num)
  {
  case 0:
    REG_AX(*regs)=newValue;
    break;
  case 1:
    REG_CX(*regs)=newValue;
    break;
  case 2:
    REG_DX(*regs)=newValue;
    break;
  case 3:
    REG_BX(*regs)=newValue;
    break;
  case 4:
    REG_SP(*regs)=newValue;
    break;
  case 5:
    REG_BP(*regs)=newValue;
    break;
  case 6:
    REG_SI(*regs)=newValue;
    break;
  case 7:
    REG_DI(*regs)=newValue;
    break;
  case 8:
    REG_IP(*regs)=newValue;
    break;
  default:
    //If this is hit, probably more need to be implemented
    fprintf(stderr,"unknown register number %i, cannot set reg val\n",num);
    death(NULL);
  }
  #elif defined(KATANA_X86_64_ARCH)
  switch(num)
  {
  case 0:
    REG_AX(*regs)=newValue;
    break;
  case 1:
    REG_DX(*regs)=newValue;
    break;
  case 2:
    REG_CX(*regs)=newValue;
    break;
  case 3:
    REG_BX(*regs)=newValue;
    break;
  case 4:
    REG_SI(*regs)=newValue;
    break;
  case 5:
    REG_DI(*regs)=newValue;
    break;
  case 6:
    REG_BP(*regs)=newValue;
    break;
  case 7:
    REG_SP(*regs)=newValue;
    break;
  case 8:
    REG_8(*regs)=newValue;
    break;
  case 9:
    REG_9(*regs)=newValue;
    break;
  case 10:
    REG_10(*regs)=newValue;
    break;
  case 11:
    REG_10(*regs)=newValue;
    break;
  case 12:
    REG_10(*regs)=newValue;
    break;
  case 13:
    REG_10(*regs)=newValue;
    break;
  case 14:
    REG_10(*regs)=newValue;
    break;
  case 15:
    REG_10(*regs)=newValue;
    break;;
  default:
    //If this is hit, probably more need to be implemented
    fprintf(stderr,"unknown register number %i, cannot set reg val\n",num);
    death(NULL);
  }
  #else
  #error "Unsupported architecture"
  #endif
}

//conversion between dwarf register numbers and x86 registers
long int getRegValueFromDwarfRegNum(struct user_regs_struct regs,int num)
{
  #ifdef KATANA_X86_ARCH
  switch(num)
  {
  case 0:
    return REG_AX(regs);
  case 1:
    return REG_CX(regs);
  case 2:
    return REG_DX(regs);
  case 3:
    return REG_BX(regs);
  case 4:
    return REG_SP(regs);
  case 5:
    return REG_BP(regs);
  case 6:
    return REG_SI(regs);
  case 7:
    return REG_DI(regs);
  case 8:
    return REG_IP(regs);
  default:
    //If this is hit, probably more need to be implemented
    fprintf(stderr,"unknown register number %i, cannot get reg val\n",num);
    death(NULL);
  }
  #elif defined(KATANA_X86_64_ARCH)
  switch(num)
  {
  case 0:
    return REG_AX(regs);
  case 1:
    return REG_DX(regs);
  case 2:
    return REG_CX(regs);
  case 3:
    return REG_BX(regs);
  case 4:
    return REG_SI(regs);
  case 5:
    return REG_DI(regs);
  case 6:
    return REG_BP(regs);
  case 7:
    return REG_SP(regs);
  case 8:
    return REG_8(regs);
  case 9:
    return REG_9(regs);
  case 10:
    return REG_10(regs);
  case 11:
    return REG_11(regs);
  case 12:
    return REG_12(regs);
  case 13:
    return REG_13(regs);
  case 14:
    return REG_14(regs);
  case 15:
    return REG_15(regs);
  default:
    //If this is hit, probably more need to be implemented
    fprintf(stderr,"unknown register number %i, cannot get reg val\n",num);
    death(NULL);
  }
  #else
  #error "Unsupported architecture"
  #endif
  return -1;
}



//actually set the registers in memory as we walk up the stack
//this function is not actually used at present, as we use libunwind.
//since this is not actually used, it may not work
struct user_regs_struct restoreRegsFromRegisterRules(struct user_regs_struct currentRegs,
                                                     Dictionary* rulesDict)
{
  struct user_regs_struct regs=currentRegs;
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  PoRegRule* cfaRule=dictGet(rulesDict,strForReg(cfaReg,0));
  if(!cfaRule)
  {
    death("no way to compute cfa\n");
  }
  assert(ERRT_CFA==cfaRule->type);
  assert(ERT_BASIC==cfaRule->regRH.type);
  uint cfaAddr=getRegValueFromDwarfRegNum(currentRegs,cfaRule->regRH.u.index)+
    cfaRule->offset;
  for(int i=0;i<=NUM_REGS;i++)
  {
    //printf("restoring reg %i\n",i);
    char buf[32];
    sprintf(buf,"r%i",i);
    PoRegRule* rule=dictGet(rulesDict,buf);
    if(!rule)
    {
      char* regName=getArchRegNameFromDwarfRegNum(i);
      if(!strcmp("esp",regName) && !strcmp("rsp",regName))
      {
        //no rule to restore the stack pointer, assume it's the same as the cfa
        REG_SP(regs)=cfaAddr;
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
      idx_t symIdx=findSymbolContainingAddress(elf,ip,STT_FUNC,SHN_UNDEF);
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
addr_t findSafeBreakpointForPatch(ElfInfo* targetBin,ElfInfo* patch,int pid,
                                  bool avoidCurrentFrame)
{
  DList* activationFrames=findActivationFrames(targetBin,pid);
  Elf_Data* unsafeFunctionsData=getDataByERS(patch,ERS_UNSAFE_FUNCTIONS);
  if(!unsafeFunctionsData)
  {
    death("Patch object does not contain any unsafe functions data. This should not be\n");
  }
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
  DList* deepestGoodFrameLi=NULL;
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
      deepestGoodFrameLi=li;
      logprintf(ELL_INFO_V1,ELS_SAFETY,"Activation frame at 0x%x (%s) passed safety check\n",((ActivationFrame*)deepestGoodFrameLi->value)->pc,getFunctionNameAtPC(targetBin,frame->pc));
    }
  }
  if(!deepestGoodFrameLi)
  {
    printBacktrace(targetBin,pid);
    death("All functions with activation frames on the stack require patching. The application will never be in a patchable state!");
  }

  if(deepestGoodFrameLi->prev && avoidCurrentFrame)
  {
    //never break in the current function if possible
    deepestGoodFrameLi = deepestGoodFrameLi->prev;
  }
  
  return ((ActivationFrame*)deepestGoodFrameLi->value)->pc;
}


void bringTargetToSafeState(ElfInfo* targetBin,ElfInfo* patch,int pid)
{
  bool avoidCurrentFrame = true;
  addr_t safeBreakpointSpot=findSafeBreakpointForPatch(targetBin,patch,pid, avoidCurrentFrame);
  logprintf(ELL_INFO_V2,ELS_PATCHAPPLY,"Setting breakpoint to apply patch at 0x%x\n",
            safeBreakpointSpot);
  setBreakpoint(safeBreakpointSpot);
  continuePtrace();
  logprintf(ELL_INFO_V2,ELS_PATCHAPPLY,"Continuing until we reach safe spot to patch. . .\n");
  bool breakpointReached=false;
  int numIterations = config.maxWaitForPatching*1e6/WAIT_FOR_SAFE_PATCHING_USLEEP;
  for(int i=0;i<numIterations;i++)
  {
    if(avoidCurrentFrame &&
       i * WAIT_FOR_SAFE_PATCHING_USLEEP > (MAX_WAIT_SECONDS_BEFORE_TRY_CURRENT_FRAME*1e6))
    {
      logprintf(ELL_INFO_V2, ELS_PATCHAPPLY,
                "Program still waiting for breakpoint, relaxing restriction on current function\n");
      kill(pid,SIGSTOP);
      waitpid(pid, NULL, WUNTRACED);
      removeBreakpoint(safeBreakpointSpot);
      avoidCurrentFrame = false;
      safeBreakpointSpot = findSafeBreakpointForPatch(targetBin, patch, pid, avoidCurrentFrame);
      setBreakpoint(safeBreakpointSpot);
      continuePtrace();
    }
    logprintf(ELL_INFO_V3,ELS_PATCHAPPLY,"Iterating waiting for breakpoint to be hit\n");
    if(0!=waitpid(-1,NULL,WNOHANG))
    {
      logprintf(ELL_INFO_V2,ELS_PATCHAPPLY,"Reached breakpoint\n");
      breakpointReached=true;
      break;
    }
    usleep(WAIT_FOR_SAFE_PATCHING_USLEEP);
  }
  if(breakpointReached)
  {
    removeBreakpoint(safeBreakpointSpot);
  }
  else
  {
    death("Program does not seem to be reaching safe state, aborting patching\n");
  }
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
      printf("0x%lx: %s\n",(unsigned long)ip,getFunctionNameAtPC(elf,ip));
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

