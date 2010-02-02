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
  //both the integer and bytes values for an argument are not valid at any one time
  //which is valid depends on the opcode
} DwarfInstruction;

typedef struct
{
  int type;//one of DW_CFA_
  int arg1;
  PoReg arg1Reg;//whether used depends on the type
  int arg2;
  PoReg arg2Reg;//whether used depends on the type
} RegInstruction;


//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr);
//the returned memory should be freed
RegInstruction* parseFDEPatchInstructions(Dwarf_Debug dbg,unsigned char* bytes,int len,
                                     int dataAlign,int codeAlign,int* numInstrs);
//some versions of dwarf.h have a misspelling
#ifdef DW_CFA_lo_user
#define DW_CFA_KATANA_do_fixups DW_CFA_lo_user+0x5
#else
#define DW_CFA_KATANA_do_fixups DW_CFA_low_user+0x5
#endif

#endif
