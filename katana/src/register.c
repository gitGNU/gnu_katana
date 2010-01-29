/*
  File: register.h
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: registers in the patching VM. Exploitation of the fact that Dwarf
               "registers" are a very flexible concept
*/

#include "register.h"
#include "dwarf_instr.h"
#include <assert.h>

PoReg readRegFromLEB128(byte* leb,uint* bytesRead)
{
  PoReg result;
  short numBytes;
  short int numSeptets;
  byte* bytes=decodeLEB128(leb,false,&numBytes,&numSeptets);
  *bytesRead=numSeptets;
  assert(numBytes>0);
  result.type=bytes[0];
  assert(result.type>ERT_PO_START && result.type<ERT_PO_END);
  switch(result.type)
  {
  case ERT_CURR_TARG_NEW:
  case ERT_CURR_TARG_OLD:
    assert(6==numBytes);//todo: diff for 64bit
    result.size=bytes[1];
    memcpy(&result.u.offset,bytes+2,4);
    break;
  case ERT_EXPR:
    assert(5==numBytes);//todo: diff for 64bit
    result.size=0;//not used
    memcpy(&result.u.offset,bytes+1,4);
    break;
  case ERT_NEW_SYM_VAL:
  case ERT_OLD_SYM_VAL:
    assert(5==numBytes);//todo: diff for 64bit
    result.size=0;//not used
    memcpy(&result.u.offset,bytes+1,4);
    break;
  default:
    die("unsupported register type\n");
  }
  return result;
}

void printReg(PoReg reg,FILE* f)
{
  assert(reg.type!=ERT_NONE);
  switch(reg.type)
  {
  case ERT_CURR_TARG_OLD:
    fprintf(f,"{CURR_TARG_OLD,0x%x bytes,0x%x off}",reg.size,reg.u.offset);
    break;
  case ERT_CURR_TARG_NEW:
    fprintf(f,"{CURR_TARG_NEW,0x%x bytes,0x%x off}",reg.size,reg.u.offset);
    break;
  case ERT_EXPR:
    //todo: print out expression
    fprintf(f,"{EXPR,0x%x off",reg.u.offset);
    break;
  case ERT_NEW_SYM_VAL:
    //todo: find symbol name
    fprintf(f,"{NEW_SYM_VAL,idx %i}",reg.u.index);
    break;
  case ERT_OLD_SYM_VAL:
    //todo: find symbol name
    fprintf(f,"{OLD_SYM_VAL,idx %i}",reg.u.index);
    break;
  default:
    death("unsupported register type\n");
  }
}
