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
#include "util.h"

byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,short int* numBytesOut);
byte* decodeLEB128(byte* bytes,bool signed_,short int* numBytesOut,short int* numSeptetsRead);


typedef struct
{
  byte* instrs;
  int numBytes;
  int allocated;
} DwarfInstructions;



typedef struct
{
  short opcode;
  byte* arg1Bytes;//LEB128 number or DW_FORM_block
  short arg1NumBytes;
  byte* arg2Bytes;
  short arg2NumBytes;
  int arg1;
  int arg2;
  //both the integer and bytes values for an argument are not valid at any one time
  //which is valid depends on the opcode
} DwarfInstruction;

//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr);

//some versions of dwarf.h have a misspelling
#ifdef DW_CFA_lo_user
#define DW_CFA_KATANA_do_fixups DW_CFA_lo_user+0x5
#else
#define DW_CFA_KATANA_do_fixups DW_CFA_low_user+0x5
#endif

#endif
