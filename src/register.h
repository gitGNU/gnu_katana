/*
  File: register.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: January 2010
  Description: registers in the patching VM. Exploitation of the fact that Dwarf
               "registers" are a very flexible concept
*/

#ifndef register_h
#define register_h
#include <stdbool.h>
#include "util/util.h"
#include "types.h"
#include "elfparse.h"

typedef enum
{
  ERT_NONE=0,
  ERT_BASIC,//registers with simple numbers as Dwarf expects them
  ERT_PO_START=0xF0,   //used to specify to any general dwarf reader that knows about patch
                      //objects that things in this range are PO register types
  ERT_CURR_TARG_NEW,
  ERT_CURR_TARG_OLD,
  ERT_EXPR,
  ERT_OLD_SYM_VAL,
  ERT_NEW_SYM_VAL,
  ERT_CFA,
  ERT_PO_END
} E_REG_TYPE;

static inline bool isPoRegType(byte b)
{
  return b>ERT_PO_START && b<ERT_PO_END;
}

typedef struct
{
  E_REG_TYPE type;
  int size;//used for ERT_CURR_TARG_*
  union
  {
    int offset;//for ERT_CURR_TARG_*, ERT_EXPR
    int index;//for ERT_*_SYM_VAL and ERT_BASIC
  } u;
} PoReg;

PoReg readRegFromLEB128(byte* leb,usint* bytesRead);

//the returned string should be freed
char* strForReg(PoReg reg);
void printReg(PoReg reg,FILE* f);



typedef enum
{
  ERRT_UNDEF=0,
  ERRT_OFFSET,
  ERRT_REGISTER,
  ERRT_CFA,
  ERRT_EXPR,
  ERRT_RECURSE_FIXUP,
  ERRT_RECURSE_FIXUP_POINTER
} E_REG_RULE_TYPE;

typedef struct
{
  PoReg regLH;
  E_REG_RULE_TYPE type;
  PoReg regRH;//not valid if type is ERRT_OFFSET
  int offset;//only valid if type is ERRT_OFFSET or ERRT_CFA or ERRT_EXPR
  idx_t index;//only valid if type is ERRT_RECURSE_FIXUP or ERRT_RECURSE_FIXUP_POINTER
} PoRegRule;

//rules are of type PoRegRule
void printRules(Dictionary* rulesDict,char* tabstr);


typedef struct
{
  addr_t currAddrOld;//corresponding to CURR_TARG_OLD register
  addr_t currAddrNew;//corresponding to CURR_TARG_NEW register
  ElfInfo* oldBinaryElf;//needed for looking up symbols
  addr_t cfaValue;
} SpecialRegsState;

typedef enum
{
  ERRF_NONE=0,
  ERRF_ASSIGN=1,
  ERRF_DEREFERENCE=2
} E_REG_RESOLVE_FLAGS;

//resolve any register to a value (as distinct from the symbolic form it may be in)
//this may include resolving symbols in elf files, dereferencing
//things in memory, etc
//the result will be written to the result parameter and the number
//of bytes in the result will be returned;
//some values behave differently if they're being assigned than evaluated
//flags determines this behaviour (should be OR'd E_REG_RESOLVE_FLAGS
int resolveRegisterValue(PoReg* reg,SpecialRegsState* state,byte** result,int flags);

#endif
