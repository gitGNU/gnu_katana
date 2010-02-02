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
#include <util/util.h>
#include "elfparse.h"
#include "patcher/target.h"
#include "register.h"
#include "util/map.h"

#define NUM_REGS 32
#define CFA_REGNUM 31


int pid=0;//pid of the target process

typedef struct
{
  int type;//one of DW_CFA_
  int arg1;
  PoReg arg1Reg;//whether used depends on the type
  int arg2;
  PoReg arg2Reg;//whether used depends on the type
} RegInstruction;


typedef struct
{
  int regnum;
  PoReg poRegLH;
  E_REG_RULE_TYPE type;
  int offset;//only valid if type is ERRT_OFFSET or ERRT_CFA
  int registerNum;//only valid if type is ERRT_OFFSET or ERRT_CFA and poReg is not valid
  PoReg poRegRH;////only valid if type is ERRT_OFFSET or ERRT_CFA and type is not ERRT_NONE
} RegRule;

typedef struct
{
  RegInstruction* initialInstructions;
  int numInitialInstructions;
  Dictionary* initialRules;
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

void printRules(Dictionary* rulesDict,char* tabstr);
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
                                           short unsigned int *leb128_length);
char* get_fde_proc_name(Dwarf_Debug dbg, Dwarf_Addr low_pc);


//evaluates the given instructions and stores them in the output regarray
//the initial condition of regarray IS taken into account
//execution continues until the end of the instructions or until the location is advanced
//past stopLocation. stopLocation should be relative to the start of the instructions (i.e. the instructions are considered to start at 0)
//if stopLocation is negative, it is ignored
void evaluateInstructions(RegInstruction* instrs,int numInstrs,Dictionary* rules,int stopLocation)
{
  RegRule* rule=NULL;
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  RegRule* cfaRule=dictGet(rules,strForReg(cfaReg));
  int loc=0;
  for(int i=0;i<numInstrs;i++)
  {
    RegInstruction inst=instrs[i];
    if(ERT_NONE==inst.arg1Reg.type)
    {
      char buf[32];
      sprintf(buf,"%i",inst.arg1);
      rule=dictGet(rules,buf);
      if(!rule)
      {
        rule=zmalloc(sizeof(RegRule));
        rule->regnum=inst.arg1;
        dictInsert(rules,buf,rule);
      }
    }
    else
    {
      rule=dictGet(rules,strForReg(inst.arg1Reg));
      if(!rule)
      {
        rule=zmalloc(sizeof(RegRule));
        rule->poRegLH=inst.arg1Reg;
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
      rule->registerNum=inst.arg2;
      rule->poRegRH=inst.arg2Reg;
      break;
    case DW_CFA_def_cfa:
      cfaRule->type=ERRT_CFA;
      cfaRule->registerNum=inst.arg1;
      cfaRule->poRegRH=inst.arg1Reg;
      cfaRule->offset=inst.arg2;
      break;
    case DW_CFA_def_cfa_register:
      cfaRule->type=ERRT_CFA;
      cfaRule->registerNum=inst.arg1;
      cfaRule->poRegRH=inst.arg1Reg;
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
    short unsigned int uleblen;
    result[*numInstrs].arg1Reg.type=ERT_NONE;//not using reg unless we set its type
    result[*numInstrs].arg2Reg.type=ERT_NONE;//not using reg unless we set its type
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
        printf("Reading CW_CFA_register\n");
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        printf("byte is %i,%i,%i\n",(int)bytes[1],(int)bytes[1],(int)bytes[3]);
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg2Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg2=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa:
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        result[*numInstrs].arg2=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        bytes+=uleblen;
        len-=uleblen;
        break;
      case DW_CFA_def_cfa_register:
        if(isPoRegType(bytes[1]))
        {
          result[*numInstrs].arg1Reg=readRegFromLEB128(bytes + 1,&uleblen);
        }
        else
        {
          result[*numInstrs].arg1=local_dwarf_decode_u_leb128(bytes + 1, &uleblen);
        }
        bytes+=uleblen;
        len-=uleblen;
        break;
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
  cie.initialRules=dictCreate(100);//todo: get rid of arbitrary constant 100
  evaluateInstructions(cie.initialInstructions,cie.numInitialInstructions,cie.initialRules,-1);
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
    printf("fde has lowpc of %i and highpc of %i\n",fdes[i].lowpc,fdes[i].highpc);
    
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
    printf("DW_CFA_register ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i ",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf(" ");
    }
    if(ERT_NONE==inst.arg2Reg.type)
    {
      printf("r%i ",inst.arg2);
    }
    else
    {
      printReg(inst.arg2Reg,stdout);
      printf(" ");
    }
    printf("\n");
    break;
  case DW_CFA_def_cfa:
    printf("DW_CFA_def_cfa ");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i ",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf(" ");
    }
    printf("%i \n",inst.arg2);
    break;
  case DW_CFA_def_cfa_register:
    printf("DW_CFA_def_cfa_register");
    if(ERT_NONE==inst.arg1Reg.type)
    {
      printf("r%i\n",inst.arg1);
    }
    else
    {
      printReg(inst.arg1Reg,stdout);
      printf("\n");
    }
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

  char* regStr=NULL;
  char buf[128];
  if(ERRT_CFA==rule.type)
  {
    regStr="cfa";
  }
  else if(rule.poRegLH.type!=ERT_NONE)
  {
    regStr=strForReg(rule.poRegLH);
  }
  else
  {
    snprintf(buf,128,"r%i(%s)",rule.regnum,getX86RegNameFromDwarfRegNum(rule.regnum));
  }
  switch(rule.type)
  {
  case ERRT_UNDEF:
    printf("%s = Undefined\n",regStr);
    break;
  case ERRT_OFFSET:
    printf("%s = cfa + %i\n",regStr,rule.offset);
    break;
  case ERRT_REGISTER:
    if(ERT_NONE!=rule.poRegRH.type)
    {
      printf("%s = %s\n",regStr,strForReg(rule.poRegRH));
    }
    else
    {
      printf("%s = r%i(%s)\n",regStr,rule.registerNum,getX86RegNameFromDwarfRegNum(rule.registerNum));
    }
    break;
  case ERRT_CFA:
    if(ERT_NONE!=rule.poRegRH.type)
    {
      printf("%s = %s + %i\n",regStr,strForReg(rule.poRegRH),rule.offset);
    }
    else
    {
      printf("%s = r%i(%s) + %i\n",regStr,rule.registerNum,getX86RegNameFromDwarfRegNum(rule.registerNum),rule.offset);
    }
    break;
  default:
    death("unknown rule type\n");
  }
}

void printRules(Dictionary* rulesDict,char* tabstr)
{
  RegRule** rules=(RegRule**)dictValues(rulesDict);
  for(int i=0;rules[i];i++)
  {
    if(rules[i]->type!=ERRT_UNDEF)
    {
      printf("%s",tabstr);
      printRule(*rules[i],i);
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
  for(int i=fde->lowpc;i<fde->highpc || 0==i;i++)
  {
    Dictionary* rulesDict=dictDuplicate(cie.initialRules,NULL);
    evaluateInstructions(fde->instructions,fde->numInstructions,rulesDict,i-fde->lowpc);
    printf("\t\t-Register Rules at text address 0x%x------\n",i);
    printRules(rulesDict,"\t\t\t");
    dictDelete(rulesDict,NULL);
  }
}

void printCallFrame(uint pc)
{
  FDE* fde=getFDEForPC(pc);
  printf("##########Call Frame##########\n");
  printFDEInfo(fde);
  Dictionary* rulesDict=dictDuplicate(cie.initialRules,NULL);
  evaluateInstructions(fde->instructions,fde->numInstructions,rulesDict,pc);
  printf("------Register Rules at PC 0x%x-------\n",pc);
  printRules(rulesDict,"\t");
  dictDelete(rulesDict,NULL);
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
                             Dictionary* rulesDict)
{
  struct user_regs_struct regs=currentRegs;
  PoReg cfaReg;
  memset(&cfaReg,0,sizeof(PoReg));
  cfaReg.type=ERT_CFA;
  RegRule* cfaRule=dictGet(rulesDict,strForReg(cfaReg));
  assert(ERRT_CFA==cfaRule->type);
  uint cfaAddr=getRegValueFromDwarfRegNum(currentRegs,cfaRule->registerNum)+cfaRule->offset;
  for(int i=0;i<NUM_REGS;i++)
  {
    //printf("restoring reg %i\n",i);
    char buf[32];
    sprintf(buf,"%i",i);
    RegRule* rule=dictGet(rulesDict,buf);
    if(!rule)
    {
      continue;
    }
    if(rule->type!=ERRT_UNDEF && i!=CFA_REGNUM)
    {
      if(ERRT_REGISTER==rule->type)
      {
        //printf("type is register\n");
        setRegValueFromDwarfRegNum(&regs,rule->registerNum,
                                   getRegValueFromDwarfRegNum(currentRegs,
                                                              rule->registerNum));
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

void printBacktrace()
{
  struct user_regs_struct regs;
  getTargetRegs(&regs);
  printf("~~~~~Backtrace:~~~~~\n");
  for(int i=0;;i++)
  {
    FDE* fde=getFDEForPC(regs.eip);
    if(!fde)
    {
      break;
    }
    printf("%i. %s\n",i,get_fde_proc_name(dbg,fde->lowpc));
    Dictionary* rulesDict=dictDuplicate(cie.initialRules,NULL);
    evaluateInstructions(fde->instructions,fde->numInstructions,rulesDict,regs.eip);    
    printf("\t{eax=0x%x,ecx=0x%x,edx=0x%x,ebx=0x%x,esp=0x%x,\n\tebp=0x%x,esi=0x%x,edi=0x%x,eip=0x%x}\n",(uint)regs.eax,(uint)regs.ecx,(uint)regs.edx,(uint)regs.ebx,(uint)regs.esp,(uint)regs.ebp,(uint)regs.esi,(uint)regs.edi,(uint)regs.eip);
    printf("\tunwinding and applying register restore rules:\n");
    printRules(rulesDict,"\t");
    regs=restoreRegsFromRegisterRules(regs,rulesDict);
    dictDelete(rulesDict,NULL);
  }
}


int main(int argc,char** argv)
{
  pid=-1;
  if(argc<2)
  {
    death("must specify elf file to open\n");
  }
  if(argc>2)
  {
    pid=atoi(argv[2]);
  }
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    die("Failed to init ELF library\n");
  }
  ElfInfo* e=openELFFile(argv[1]);
  Dwarf_Error err;
  if(DW_DLV_OK!=dwarf_elf_init(e->e,DW_DLC_READ,&dwarfErrorHandler,NULL,&dbg,&err))
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
    byte code[]={0xcc,0x90,0x90,0x90};
    byte oldData[4];//what we replaced
    uint locToReplace=0x80484fd;//address of a statement in function foo4
    memcpyFromTarget(oldData,locToReplace,4);
    memcpyToTarget(locToReplace,code,4);
    printf("data we're replacing is %02x,%02x,%02x,%02x\n",oldData[0],oldData[1],oldData[2],oldData[3]);
    printf("set up breakpoint\n");
    continuePtrace();
    wait(NULL);
    struct user_regs_struct regs;
    getTargetRegs(&regs);
    printf("proc stopped at 0x%x. Press any key to start it again\n",(unsigned int)regs.eip);
    //printCallFrame(regs.eip);
    printBacktrace();
    getc(stdin);
    //put things back
    memcpyToTarget(locToReplace,oldData,4);
    printf("put back old info eip=0x%x locToReplace=0x%x\n",(uint)regs.eip,(uint)locToReplace);
    regs.eip=locToReplace;//so can execute that instruction normally
    setTargetRegs(&regs);
    struct user_regs_struct newRegs;
    getTargetRegs(&newRegs);
    if(memcmp(&regs,&newRegs,sizeof(struct user_regs_struct)))
    {
      death("something wrong\n");
    }
    printf("put back instruction pointer\n");
    printf("the process should continue running\n");
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
