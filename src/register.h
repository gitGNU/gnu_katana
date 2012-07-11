/*
  File: register.h
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
  Date: January 2010
  Description: registers in the patching VM. Exploitation of the fact that Dwarf
               "registers" are a very flexible concept
*/

#ifndef register_h
#define register_h
#include <stdbool.h>
#include "util/util.h"
#include "types.h"

typedef enum
{
  ERT_NONE=0,
  ERT_BASIC,//registers with simple numbers as Dwarf expects them
  ERT_PO_START=0xF0,   //used to specify to any general dwarf reader that knows about patch
                      //objects that things in this range are PO register types
  ERT_CURR_TARG_NEW,
  ERT_CURR_TARG_OLD,
  //todo: do expressions need to be a register type? Can they be their
  //own entity? Since we don't need them (yet) for patching this is a
  //moot question at the moment
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

PoReg* copyPoReg(PoReg* old);


PoReg readRegFromLEB128(byte* leb,usint* bytesRead);

//return value should be freed when caller is finished with it
byte* encodeRegAsLEB128(PoReg reg,bool signed_,usint* numBytesOut);

//printFlags should be OR'd DwarfInstructionPrintFlag
//the returned string should be freed
char* strForReg(PoReg reg,int printFlags);

//printFlags should be OR'd DwarfInstructionPrintFlag
void printReg(FILE* f,PoReg reg,int printFlags);



typedef enum
{
  ERRT_UNDEF=0,
  ERRT_OFFSET,
  ERRT_REGISTER,
  ERRT_CFA,
  ERRT_EXPR,
  ERRT_RECURSE_FIXUP,
  ERRT_RECURSE_FIXUP_POINTER,
  ERRT_UNDEFINED
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
void printRules(FILE* file,Dictionary* rulesDict,char* tabstr);


typedef struct
{
  addr_t currAddrOld;//corresponding to CURR_TARG_OLD register
  addr_t currAddrNew;//corresponding to CURR_TARG_NEW register
  struct ElfInfo* oldBinaryElf;//needed for looking up symbols
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

char* getArchRegNameFromDwarfRegNum(int num);

PoRegRule* duplicatePoRegRule(PoRegRule* rule);

PoReg* duplicatePoReg(PoReg* old);
#endif
