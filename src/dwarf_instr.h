/*
  File: dwarf_instr.h
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
  Description: Functions for manipulating dwarf instructions
*/

#ifndef dwarf_instr_h
#define dwarf_instr_h

#include <stdbool.h>
#include "util/util.h"
#include <libdwarf.h>
#include <dwarf.h>
#include "register.h"


//the DwarfInstructions and DwarfInstruction structs are used when
//writing information to DWARF binary data when producing a new ELF file.
typedef struct
{
  byte* instrs;
  uint numBytes;
  int allocated;
} DwarfInstructions;


typedef struct
{
  short opcode;
  byte* arg1Bytes;//LEB128 number, DW_FORM_block, or just straight up
                  //address or offset depending on opcode
  usint arg1NumBytes;
  byte* arg2Bytes;
  usint arg2NumBytes;
  word_t arg1;
  word_t arg2;
  byte* arg3Bytes;
  usint arg3NumBytes;
  word_t arg3;
  //both the integer and bytes values for an argument are not valid at any one time
  //which is valid depends on the opcode
} DwarfInstruction;

//representation of a single instruction in a DWARf Expression
typedef struct
{
  int type;//one of DW_OP_*
  word_t arg1;
  word_t arg2;
} DwarfExprInstr;

//representation of a DWARf Expression
typedef struct
{
  DwarfExprInstr* instructions;
  int numInstructions;
} DwarfExpr;

void addToDwarfExpression(DwarfExpr* expr,DwarfExprInstr instr);


//This struct is used when parsing DWARF binary information into a
//structure we can deal with. How is it different on a high level from
//DwarfInstruction? It isn't really, it just has some more information
//about registers to facilitate evaluating the instruction. Naming
//conventions should probably be changed to be a little more
//sane/explanatory.
typedef struct
{
  int type;//one of DW_CFA_
  word_t arg1;//whether used depends on the type
  DwarfExpr expr;//only used for DW_CFA_expression and similar
  PoReg arg1Reg;//whether used depends on the type
  word_t arg2;//whether used depends on the type
  PoReg arg2Reg;//whether used depends on the type
  word_t arg3;//used only for DW_CFA_KATANA_do_fixups
} RegInstruction;


//add a new instruction to an array of instructions
void addInstruction(DwarfInstructions* instrs,DwarfInstruction* instr);

typedef enum _DwarfInstructionPrintFlags
{
  DWIPF_NO_REG_NAMES=1,
  DWIPF_DWARFSCRIPT=2 //we're printing things for purposes of dwarfscript
} DwarfInstructionPrintFlags;

//printing flags should be OR'd DwarfInstructionPrintFlags
void printInstruction(FILE* file,RegInstruction inst,int printFlags);
void printExpr(FILE* file,char* prefix,DwarfExpr expr,int printFlags);

void destroyRawInstructions(DwarfInstructions instrs);

//convert the higher-level RegInstruction format into the raw string
//of binary bytes that is the DwarfInstructions structure
DwarfInstructions serializeDwarfRegInstructions(RegInstruction* regInstrs,int numRegInstrs);

//encodes a Dwarf Expression as a LEB-encoded length followed
//by length bytes of Dwarf expression
//the returned pointer should be freed
byte* encodeDwarfExprAsFormBlock(DwarfExpr expr,usint* numBytesOut);

extern char** dwarfExpressionNames;

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
