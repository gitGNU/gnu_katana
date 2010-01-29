/*
  File: register.h
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: registers in the patching VM. Exploitation of the fact that Dwarf
               "registers" are a very flexible concept
*/

#ifndef register_h
#define register_h
#include <stdbool.h>
#include "util.h"
typedef enum
{
  ERT_NONE=0,
  ERT_PO_START=0xF0,   //used to specify to any general dwarf reader that knows about patch
                      //objects that things in this range are PO register types
  ERT_CURR_TARG_NEW,
  ERT_CURR_TARG_OLD,
  ERT_EXPR,
  ERT_OLD_SYM_VAL,
  ERT_NEW_SYM_VAL,
  ERT_PO_END
} E_REG_TYPE;

static inline bool isPoRegType(byte b)
{
  return b>ERT_PO_START && b<ERT_PO_END;
}

typedef struct
{
  E_REG_TYPE type;
  byte size;//used for ERT_CURR_TARG_*
  union
  {
    int offset;//for ERT_CURR_TARG_*, ERT_EXPR
    int index;//for ERT_*_SYM_VAL
  } u;
} PoReg;

PoReg readRegFromLEB128(byte* leb,uint* bytesRead);
void printReg(PoReg reg,FILE* f);
#endif
