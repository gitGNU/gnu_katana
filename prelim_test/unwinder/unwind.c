/*
  File: unwind.c
  Author: James Oakley
  Project: Katana - preliminary testing
  Date: January 10
  Description: Program to print dwarf information necessary to unwind the stack,
               to demonstrate how stack unwinding is done
  License:  This code is based heavily on GPL code from Dwarfdump, therefore it
            must be kept under the GPL
*/

//note: according to the dwarf spec nearly everything is LEB128. We use ints here for convenience

//note: not all instruction types are covered, only the ones I've seen gcc generate
//there are others that would be highly useful if we actually use the dwarf data format to describe patching
#include <sys/wait.h>
#include <libdwarf.h>
#include <dwarf.h>
#include <assert.h>
#include <util.h>
#include "elfparse.h"
#include "target.h"


#define NUM_REGS 32
#define CFA_REGNUM 31

int pid=0;//pid of the target process

typedef struct
{
  int type;//one of DW_CFA_
  int arg1;
  int arg2;
} RegInstruction;

typedef enum
{
  ERT_UNDEF=0,
  ERT_OFFSET,
  ERT_REGISTER,
  ERT_CFA
} E_RULE_TYPE;

typedef struct
{
  int regnum;
  E_RULE_TYPE type;
  int offset;//only valid if type is ERT_OFFSET or ERT_CFA
  int registerNum;//only valid if type is ERT_OFFSET or ERT_CFA
} RegRule;

typedef struct
{
  RegInstruction* initialInstructions;
  int numInitialInstructions;
  RegRule initialRules[NUM_REGS];
  Dwarf_Signed dataAlign;
  Dwarf_Unsigned codeAlign;
  Dwarf_Half returnAddrRuleNum;
} CIE;


typedef struct
{
  Dwarf_Fde dfde;
  CIE* cie;
  RegInstruction* instructions;
  int numInstructions;
  int lowpc;
  int highpc;
} FDE;

void printRules(RegRule* rules,char* tabstr);
void printRule(RegRule rule,int regnum);
char* getX86RegNameFromDwarfRegNum(int num);

Dwarf_Fde *fdeData = NULL;
Dwarf_Signed fdeElementCount = 0;
Dwarf_Cie *cieData = NULL;
Dwarf_Signed cieElementCount = 0;
CIE cie;
FDE* fdes;
int numFdes;
Dwarf_Debug dbg;

Dwarf_Unsigned local_dwarf_decode_u_leb128(unsigned char *leb128,
                                           unsigned int *leb128_length);
char* get_fde_proc_name(Dwarf_Debug dbg, Dwarf_Addr low_pc);


//evaluates the given instructions and stores them in the output regarray
//the initial condition of regarray IS taken into account
//execution continues until the end of the instructions or until the location is advanced
//past stopLocation. stopLocation should be relative to the start of the instructions (i.e. the instructions are considered to start at 0)
//if stopLocation is negative, it is ignored
void evaluateInstructions(RegInstruction* instrs,int numInstrs,RegRule  rules[NUM_REGS],int stopLocation,CIE* cie)
{
  int loc=0;
  for(int i=0;i<numInstrs;i++)
  {
    RegInstruction inst=instrs[i];
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
      assert(inst.arg1<NUM_REGS);
      rules[inst.arg1].type=ERT_OFFSET;
      rules[inst.arg1].offset=inst.arg2;
      break;
    case DW_CFA_register:
      assert(inst.arg1<NUM_REGS);
      rules[inst.arg1].type=ERT_REGISTER;
      rules[inst.arg1].registerNum=inst.arg2;
      break;
    case DW_CFA_def_cfa:
      rules[CFA_REGNUM].type=ERT_CFA;
      rules[CFA_REGNUM].registerNum=inst.arg1;
      rules[CFA_REGNUM].offset=inst.arg2;
      break;
    case DW_CFA_def_cfa_register:
      rules[CFA_REGNUM].type=ERT_CFA;
      rules[CFA_REGNUM].registerNum=inst.arg1;
      break;
    case DW_CFA_def_cfa_offset:
      rules[CFA_REGNUM].type=ERT_CFA;
      rules[CFA_REGNUM].offset=inst.arg1;
      break;
    case DW_CFA_nop:
      //do nothing, nothing changed
      break;
    default:
      death("unexpected instruction");
    }
  }
}

//the returned memory should be freed
RegInstruction* parseFDEInstructions(Dwarf_Debug dbg,unsigned char* bytes,int len,
                                     int dataAlign,int codeAlign,int* numInstrs)
{
  *numInstrs=0;
  //allocate more mem than we'll actually need
  //we can free some later
  RegInstruction* result=zmalloc(sizeof(RegInstruction)*len);
  for(;len>0;len--,bytes++,(*numInstrs)++)
  {
    //as dwarfdump does, separate out high and low portions
    //of the byte based on the boundaries of the instructions that
    //encode an operand with the instruction
    //all other instructions are accounted for only by the bottom part of the byte
    unsigned char byte=*bytes;
    int high = byte & 0xc0;
    int low = byte & 0x3f;
    uint uleblen;
    switch(high)
    {
    case DW_CFA_advance_loc:
      result[*numInstrs].type=high;
      result[*numInstrs].arg1=low*codeAlign;
      break;
    case DW_CFA_offset:
      result[*numInstrs].type=high;
      result[*numInstrs].arg1=low;
      result[*numInstrs].arg2=local_dwarf_decode_u_leb128(bytes + 1, &uleblen)*dataAlign;
      bytes+=uleblen;
      len-=uleblen;
      break;
    case DW_CFA_restore:
      death("DW_CFA_restore not handled\n");
      break;
    default:
      //deal with the things encoded by the bottom portion
      result[*numInstrs].type=low;
      switch(low)
      {
      case DW_CFA_set_loc:
        //todo: this assumes 32-bit
        memcpy(&result[*numInstrs].arg1,bytes+1,sizeof(int));
        bytes+=sizeof(int);
        len-=sizeof(int);
        break;
      case DW_CFA_advance_loc1:
        {
        unsigned char delta = (unsigned char) *(bytes + 1);
        result[*numInstrs].arg1=delta*codeAlign;
        bytes+=1;
        len -= 1;
        }
      case DW_CFA_advance_loc2:
        {
        unsigned short delta = (unsigned short) *(bytes + 1);
        result[*numInstrs].arg1=delta*codeAlign;
        bytes+=2;
        len -= 2;
        }
        break;
      case DW_CFA_register:
      case DW_CFA_def_cfa:
        result[*numInstrs].arg1=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg2=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa_register:
      case DW_CFA_def_cfa_offset:
        result[*numInstrs].arg1=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_nop:
        break;
      default:
        fprintf(stderr,"dwarf cfa instruction 0x%x not yet handled\n",(uint)low);
        death(NULL);
      }
    }
  }
  //realloc to free mem we didn't actually use
  result=realloc(result,sizeof(RegInstruction)*(*numInstrs));
  return result;
}


void dwarfErrorHandler(Dwarf_Error err,Dwarf_Ptr arg)
{
  fprintf(stderr,"Dwarf error: %s\n",dwarf_errmsg(err));
  fflush(stderr);
  abort();
}

void readDebugFrame(Dwarf_Debug dbg)
{
  Dwarf_Error err;
  dwarf_get_fde_list(dbg, &cieData, &cieElementCount,
                                   &fdeData, &fdeElementCount, &err);

  
  //read the CIE
  Dwarf_Unsigned cie_length = 0;
  Dwarf_Small version = 0;
  char* augmenter = "";
  Dwarf_Ptr initInstr = 0;
  Dwarf_Unsigned initInstrLen = 0;
  assert(1==cieElementCount);
  //todo: all the casting here is hackish
  //should respect types more
  dwarf_get_cie_info(cieData[0],
                     &cie_length,
                     &version,
                     &augmenter,
                     &cie.codeAlign,
                     &cie.dataAlign,
                     &cie.returnAddrRuleNum,
                     &initInstr,
                     &initInstrLen, &err);
  cie.initialInstructions=parseFDEInstructions(dbg,initInstr,initInstrLen,cie.dataAlign,
                                                cie.codeAlign,&cie.numInitialInstructions);
  evaluateInstructions(cie.initialInstructions,cie.numInitialInstructions,cie.initialRules,-1,&cie);
  //todo: bizarre bug, it keeps coming out as -1, which is wrong
  cie.codeAlign=1;
  
  fdes=zmalloc(fdeElementCount*sizeof(FDE));
  numFdes=fdeElementCount;
  for (int i = 0; i < fdeElementCount; i++)
  {
    Dwarf_Fde dfde=fdeData[i];
    fdes[i].dfde=dfde;
    Dwarf_Ptr instrs;
    Dwarf_Unsigned ilen;
    dwarf_get_fde_instr_bytes(dfde, &instrs, &ilen, &err);
    fdes[i].instructions=parseFDEInstructions(dbg,instrs,ilen,cie.dataAlign,cie.codeAlign,&fdes[i].numInstructions);
    Dwarf_Addr lowPC = 0;
    Dwarf_Unsigned funcLength = 0;
    Dwarf_Ptr fdeBytes = NULL;
    Dwarf_Unsigned fdeBytesLength = 0;
    Dwarf_Off cie_offset = 0;
    Dwarf_Signed cie_index = 0;
    Dwarf_Off fde_offset = 0;
    dwarf_get_fde_range(dfde,
                        &lowPC, &funcLength,
                        &fdeBytes,
                        &fdeBytesLength,
                        &cie_offset, &cie_index,
                        &fde_offset, &err);
    fdes[i].lowpc=lowPC;
    fdes[i].highpc=lowPC+funcLength;
    
  }
}

//returns null if could not be found
FDE* getFDEForPC(uint pc)
{
  //todo: horrible way to do this in a real system
  //would do something like an interval tree so not linear search time
  for(int i=0;i<fdeElementCount;i++)
  {
    FDE fde=fdes[i];
    if(fde.lowpc<=pc && fde.highpc>pc)
    {
      return fdes+i;
    }
  }

  return NULL;
}


void printInstruction(RegInstruction inst)
{
  switch(inst.type)
  {
  case DW_CFA_set_loc:
    printf("DW_CFA_set_loc %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc:
    printf("DW_CFA_advance_loc %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc1:
    printf("DW_CFA_advance_loc_1 %i\n",inst.arg1);
    break;
  case DW_CFA_advance_loc2:
    printf("DW_CFA_advance_loc_2 %i\n",inst.arg1);
    break;
  case DW_CFA_offset:
    printf("DW_CFA_offset r%i %i\n",inst.arg1,inst.arg2);
    break;
  case DW_CFA_register:
    printf("DW_CFA_register r%i r%i\n",inst.arg1,inst.arg2);
    break;
  case DW_CFA_def_cfa:
    printf("DW_CFA_def_cfa r%i %i\n",inst.arg1,inst.arg2);
    break;
  case DW_CFA_def_cfa_register:
    printf("DW_CFA_def_cfa_register r%i\n",inst.arg1);
    break;
  case DW_CFA_def_cfa_offset:
    printf("DW_CFA_def_cfa_offset %i\n",inst.arg1);
    break;
  case DW_CFA_nop:
    printf("DW_CFA_nop\n");
    break;
  default:
    death("unsupported instruction");
  }
}

void printRule(RegRule rule,int regnum)
{
  
  switch(rule.type)
  {
  case ERT_UNDEF:
    printf("r%i(%s) = Undefined\n",regnum,getX86RegNameFromDwarfRegNum(regnum));
    break;
  case ERT_OFFSET:
    printf("r%i(%s) = cfa + %i\n",regnum,getX86RegNameFromDwarfRegNum(regnum),rule.offset);
    break;
  case ERT_REGISTER:
    printf("r%i(%s) = r%i(%s)\n",regnum,getX86RegNameFromDwarfRegNum(regnum),rule.registerNum,getX86RegNameFromDwarfRegNum(rule.registerNum));
    break;
  case ERT_CFA:
    printf("cfa = r%i(%s) + %i\n",rule.registerNum,getX86RegNameFromDwarfRegNum(rule.registerNum),rule.offset);
    break;
  default:
    death("unknown rule type\n");
  }
}

void printRules(RegRule* rules,char* tabstr)
{
  for(int i=0;i<NUM_REGS;i++)
  {
    if(rules[i].type!=ERT_UNDEF)
    {
      printf("%s",tabstr);
      printRule(rules[i],i);
    }
  }
}

void printCIEInfo()
{
  printf("------CIE-----\n");
  printf("\tdata align: %i\n",(int)cie.dataAlign);
  printf("\tcode align: %u\n",(unsigned int)cie.codeAlign);
  printf("\trule for return address: %i\n",cie.returnAddrRuleNum);
  printf("\tInitial instructions:\n");
  for(int i=0;i<cie.numInitialInstructions;i++)
  {
    printf("\t\t");
    printInstruction(cie.initialInstructions[i]);
  }
  printf("\tInitial register rules (computed from instructions)\n");
  printRules(cie.initialRules,"\t\t");
}

void printFDEInfo(FDE* fde)
{
  printf("--------FDE-----\n");
  printf("\tfunction guess:%s\n",get_fde_proc_name(dbg,fde->lowpc));
  printf("\tlowpc: 0x%x\n",fde->lowpc);
  printf("\thighpc:0x%x\n",fde->highpc);
  printf("\tInstructions:\n");
  for(int i=0;i<fde->numInstructions;i++)
  {
    printf("\t\t");
    printInstruction(fde->instructions[i]);
  }
  printf("\t\tThe table would be as follows\n");
  for(int i=fde->lowpc;i<fde->highpc;i++)
  {
    RegRule regs[NUM_REGS];
    memcpy(regs,cie.initialRules,sizeof(RegRule)*NUM_REGS);
    evaluateInstructions(fde->instructions,fde->numInstructions,regs,i-fde->lowpc,&cie);
    printf("\t\t-Register Rules at text address 0x%x------\n",i);
    printRules(regs,"\t\t\t");
  }
}

void printCallFrame(uint pc)
{
  FDE* fde=getFDEForPC(pc);
  printf("##########Call Frame##########\n");
  printFDEInfo(fde);
  RegRule regs[NUM_REGS];
  memcpy(regs,cie.initialRules,sizeof(RegRule)*NUM_REGS);
  evaluateInstructions(fde->instructions,fde->numInstructions,regs,pc,&cie);
  printf("------Register Rules at PC 0x%x-------\n",pc);
  printRules(regs,"\t");
  
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

struct user_regs_struct
restoreRegsFromRegisterRules(struct user_regs_struct currentRegs,
                             RegRule* rules)
{
  struct user_regs_struct regs=currentRegs;
  RegRule cfaRule=rules[CFA_REGNUM];
  assert(ERT_CFA==cfaRule.type);
  uint cfaAddr=getRegValueFromDwarfRegNum(currentRegs,cfaRule.registerNum)+cfaRule.offset;
  for(int i=0;i<NUM_REGS;i++)
  {
    //printf("restoring reg %i\n",i);
    if(rules[i].type!=ERT_UNDEF && i!=CFA_REGNUM)
    {
      if(ERT_REGISTER==rules[i].type)
      {
        //printf("type is register\n");
        setRegValueFromDwarfRegNum(&regs,rules[i].registerNum,
                                   getRegValueFromDwarfRegNum(currentRegs,
                                                              rules[i].registerNum));
      }
      else if(ERT_OFFSET==rules[i].type)
      {
        uint addr=(int)cfaAddr + rules[i].offset;
        long int value;
        memcpyFromTarget((char*)&value,addr,sizeof(value));
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

void printBacktrace()
{
  struct user_regs_struct* regs=getTargetRegs();
  printf("~~~~~Backtrace:~~~~~\n");
  for(int i=0;;i++)
  {
    FDE* fde=getFDEForPC(regs->eip);
    if(!fde)
    {
      break;
    }
    printf("%i. %s\n",i,get_fde_proc_name(dbg,fde->lowpc));
    RegRule rules[NUM_REGS];
    memcpy(rules,cie.initialRules,sizeof(RegRule)*NUM_REGS);
    evaluateInstructions(fde->instructions,fde->numInstructions,rules,regs->eip,&cie);    
    printf("\t{eax=0x%x,ecx=0x%x,edx=0x%x,ebx=0x%x,esp=0x%x,\n\tebp=0x%x,esi=0x%x,edi=0x%x,eip=0x%x}\n",(uint)regs->eax,(uint)regs->ecx,(uint)regs->edx,(uint)regs->ebx,(uint)regs->esp,(uint)regs->ebp,(uint)regs->esi,(uint)regs->edi,(uint)regs->eip);
    printf("\tunwinding and applying register restore rules:\n");
    printRules(rules,"\t");
    *regs=restoreRegsFromRegisterRules(*regs,rules);
  }
}


int main(int argc,char** argv)
{
  pid=-1;
  if(argc>1)
  {
    pid=atoi(argv[1]);
  }
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    die("Failed to init ELF library\n");
  }
  Elf* e=openELFFile("testprog");
  Dwarf_Error err;
  if(DW_DLV_OK!=dwarf_elf_init(e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  readDebugFrame(dbg);
  printCIEInfo();
  for(int i=0;i<numFdes;i++)
  {
    printFDEInfo(fdes+i);
  }
  //printCallFrame(0x080485fd);
  
  if(pid>0)
  {
    startPtrace();
    //int3 instruction w/syscall
    //make a breakpoint at foo4
    char code[]={0xcc,0x90,0x90,0x90};
    char oldData[4];//what we replaced
    uint locToReplace=0x80484fd;//address of a statement in function foo4
    memcpyFromTarget(oldData,locToReplace,4);
    memcpyToTarget(locToReplace,code,4);
    printf("data we're replacing is %02x,%02x,%02x,%02x\n",oldData[0],oldData[1],oldData[2],oldData[3]);
    printf("set up breakpoint\n");
    continuePtrace();
    wait(NULL);
    struct user_regs_struct* regs=getTargetRegs();
    printf("proc stopped at 0x%x. Press any key to start it again\n",(unsigned int)regs->eip);
    //printCallFrame(regs.eip);
    printBacktrace();
    getc(stdin);
    //put things back
    memcpyToTarget(locToReplace,oldData,4);
    printf("put back old info eip=0x%x locToReplace=0x%x\n",regs->eip,locToReplace);
    regs->eip=locToReplace;//so can execute that instruction normally
    setTargetRegs(regs);
    struct user_regs_struct* newRegs=getTargetRegs();
    if(memcmp(regs,newRegs,sizeof(struct user_regs_struct)))
    {
      death("something wrong\n");
    }
    printf("put back instruction pointer\n");
    printf("the process may crash now, this program has a bug\n");
    memcpyFromTarget(oldData,locToReplace,4);
    printf("has bytes %02x,%02x,%02x,%02x\n",oldData[0],oldData[1],oldData[2],oldData[3]);
    endPtrace();
  }
  if(DW_DLV_OK!=dwarf_finish(dbg,&err))
  {
    dwarfErrorHandler(err,NULL);
  }
  return 0;
}
