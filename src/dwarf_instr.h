/*
  File: dwarf_instr.h
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Functions for manipulating dwarf instructions
*/

#ifndef dwarf_instr_h
#define dwarf_instr_h

#include <stdbool.h>
#include "util/util.h"
#include <libdwarf.h>
#include <dwarf.h>
#include "register.h"

byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,usint* numBytesOut);
byte* decodeLEB128(byte* bytes,bool signed_,usint* numBytesOut,usint* numSeptetsRead);
uint leb128ToUInt(byte* bytes,usint* outLEBBytesRead);


typedef struct
{
  byte* instrs;
  uint numBytes;
  int allocated;
} DwarfInstructions;



typedef struct
{
  short opcode;
  byte* arg1Bytes;//LEB128 number or DW_FORM_block
  usint arg1NumBytes;
  byte* arg2Bytes;
  usint arg2NumBytes;
  int arg1;
  int arg2;
  byte* arg3Bytes;
  usint arg3NumBytes;
  int arg3;
  //both the integer and bytes values for an argument are not valid at any one time
  //which is valid depends on the opcode
} DwarfInstruction;

typedef struct
{
  int type;//one of DW_CFA_
  int arg1;
  PoReg arg1Reg;//whether used depends on the type
  word_t arg2;//whether used depends on the type
  PoReg arg2Reg;//whether used depends on the type
  word_t arg3;//used only for DW_CFA_KATANA_do_fixups
} RegInstruction;


//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr);

void printInstruction(RegInstruction inst);

//some versions of dwarf.h have a misspelling
#if !DW_CFA_lo_user
#define DW_CFA_lo_user DW_CFA_low_user
#endif
//specifies that the value for the given register should be computed
//by applying the fixups in the given FDE It therefore takes three
//LEB128 operands, the first is a register number to be assigned, the
//second is a register to use as the "CURR_TARG_OLD" during the fixup,
//and the third is an FDE number (index of the FDE to use for fixups)
#define DW_CFA_KATANA_fixups DW_CFA_lo_user+0x5

//same thing except it is understood that the register to be assigned actually
//corresponds to a pointer to a type given by the index of the FDE
#define DW_CFA_KATANA_fixups_pointer DW_CFA_lo_user+0x6

#endif
