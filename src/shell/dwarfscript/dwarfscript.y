%{
#include "parse_helper.h"
#include "callFrameInfo.h"
#include "util/logging.h"
CallFrameInfo cfi;
CallFrameInfo* parsedCallFrameInfo=&cfi; 
FDE* currentFDE;
CIE* currentCIE;
LSDA* currentLSDA;
CallSiteRecord* currentCallSite;
ActionRecord* currentAction; 
//used when reading a DWARF expression 
DwarfExpr currentExpr;
 
#define YYSTYPE ParseNode
#define YYDEBUG 1

extern int yydwlex();

int dwLineNumber=1;
 //values saved by the lexer
char* savedString;
int savedRegisterNumber;
int savedInt;
double savedDouble;
int savedDataLen;
byte* savedData;

//save it here since we don't store it directly as part of the CIE
static char* augmentationString;

int yyerror(char *s);
void makeBasicRegister1(ParseNode* node,ParseNode* intvalNode);
void makeBasicRegister2(ParseNode* node,ParseNode* intvalNode);

 %}

%expect 1

//different prefix to avoid conflicting with our other parser
%name-prefix "yydw"

 ////////////////////
//token definitions

//literals
%token T_BOOL_TRUE T_BOOL_FALSE T_STRING_LITERAL T_HEXDATA
%token T_POS_INT T_NONNEG_INT T_INT T_FLOAT T_REGISTER
%token T_DW_PTR_ENCODING T_NONE

// basic structure
%token T_BEGIN T_END T_INVALID_TOKEN
// section types
%token T_FDE T_CIE T_INSTRUCTIONS
%token T_LSDA_ T_CALL_SITE T_ACTION T_TYPE 
//properties
%token T_INDEX T_DATA_ALIGN T_CODE_ALIGN T_RET_ADDR_RULE T_AUGMENTATION
%token T_INITIAL_LOCATION  T_ADDRESS_RANGE T_OFFSET T_LENGTH T_CIE_INDEX
%token T_VERSION T_ADDRESS_SIZE T_SEGMENT_SIZE 
%token T_AUGMENTATION_DATA T_SECTION_TYPE T_SECTION_LOC T_EH_HDR_LOC T_EXCEPT_TABLE_ADDR
%token T_LPSTART T_POSITION T_LANDING_PAD T_HAS_ACTION T_FIRST_ACTION
%token T_TYPE_IDX T_NEXT T_TYPEINFO T_TYPEINFO_ENC T_LSDA_IDX
%token T_FDE_PTR_ENC T_FDE_LSDA_PTR_ENC T_PERSONALITY_PTR_ENC T_PERSONALITY
//CFA instructions
%token T_DW_CFA_offset T_DW_CFA_advance_loc T_DW_CFA_restore T_DW_CFA_extended
%token T_DW_CFA_nop T_DW_CFA_set_loc T_DW_CFA_advance_loc1 T_DW_CFA_advance_loc2
%token T_DW_CFA_advance_loc4 T_DW_CFA_offset_extended T_DW_CFA_restore_extended
%token T_DW_CFA_undefined T_DW_CFA_same_value T_DW_CFA_register
%token T_DW_CFA_remember_state T_DW_CFA_restore_state T_DW_CFA_def_cfa
%token T_DW_CFA_def_cfa_register T_DW_CFA_def_cfa_offset
%token T_DW_CFA_def_cfa_expression T_DW_CFA_expression T_DW_CFA_offset_extended_sf
%token T_DW_CFA_def_cfa_sf  T_DW_CFA_def_cfa_offset_sf T_DW_CFA_val_offset
%token T_DW_CFA_val_offset_sf T_DW_CFA_val_expression
// DWARF expression instructions
%token T_DWARF_EXPR
%token T_DW_OP_addr T_DW_OP_deref T_DW_OP_const1u T_DW_OP_const1s T_DW_OP_const2u
%token T_DW_OP_const2s T_DW_OP_const4u T_DW_OP_const4s T_DW_OP_const8u
%token T_DW_OP_const8s T_DW_OP_constu T_DW_OP_consts T_DW_OP_dup T_DW_OP_drop
%token T_DW_OP_over T_DW_OP_pick T_DW_OP_swap T_DW_OP_rot T_DW_OP_xderef
%token T_DW_OP_abs T_DW_OP_and T_DW_OP_div T_DW_OP_minus T_DW_OP_mod T_DW_OP_mul
%token T_DW_OP_neg T_DW_OP_not T_DW_OP_or T_DW_OP_plus T_DW_OP_plus_uconst
%token T_DW_OP_shl T_DW_OP_shr T_DW_OP_shra T_DW_OP_xor T_DW_OP_skip T_DW_OP_bra
%token T_DW_OP_eq T_DW_OP_ge T_DW_OP_gt T_DW_OP_le T_DW_OP_lt
%token T_DW_OP_ne T_DW_OP_litn
%token T_DW_OP_regn T_DW_OP_bregn T_DW_OP_regx T_DW_OP_fbreg T_DW_OP_bregx
%token T_DW_OP_piece T_DW_OP_deref_size T_DW_OP_xderef_size T_DW_OP_nop
%token T_DW_OP_push_object_address T_DW_OP_call2 T_DW_OP_call4
%token T_DW_OP_call_ref T_DW_OP_form_tls_address T_DW_OP_call_frame_cfa
%token T_DW_OP_bit_piece T_DW_OP_implicit_value T_DW_OP_stack_value
%%


dwarfscript : top_property_stmt_list section_list
{}

section_list : section_list fde_section
{}
| section_list cie_section {}
| section_list lsda_section {}
| /*empty*/ {}

fde_section : fde_begin_stmt fde_property_stmt_list instruction_section fde_property_stmt_list fde_end_stmt
{}

cie_section : cie_begin_stmt cie_property_stmt_list instruction_section cie_property_stmt_list cie_end_stmt
{}

instruction_section : T_BEGIN T_INSTRUCTIONS instruction_stmt_list T_END T_INSTRUCTIONS
{
  $$=$3;
}

expr_begin_stmt : T_BEGIN T_DWARF_EXPR
{
  memset(&currentExpr,0,sizeof(currentExpr));
}

expression_section : expr_begin_stmt expr_stmt_list T_END T_DWARF_EXPR
{
  $$.u.expr=currentExpr;
}

fde_begin_stmt : T_BEGIN T_FDE
{
  cfi.fdes=realloc(cfi.fdes,(cfi.numFDEs+1)*sizeof(FDE));
  currentFDE=cfi.fdes+cfi.numFDEs;
  memset(currentFDE,0,sizeof(FDE));
  currentFDE->idx=cfi.numFDEs;
  cfi.numFDEs++;
}

fde_end_stmt : T_END T_FDE
{
  //do a sanity check on the FDE
  if(!currentFDE->cie)
  {
    fprintf(stderr,"FDE section must specify the CIE associated with the FDE\n");
    YYERROR;
  }
  currentFDE=NULL;
}

cie_begin_stmt : T_BEGIN T_CIE
{
  cfi.cies=realloc(cfi.cies,(cfi.numCIEs+1)*sizeof(CIE));
  currentCIE=cfi.cies+cfi.numCIEs;
  memset(currentCIE,0,sizeof(CIE));
  currentCIE->idx=cfi.numCIEs;
  cfi.numCIEs++;
}

cie_end_stmt : T_END T_CIE
{
  if(currentCIE->addressSize==0)
  {
    if(cfi.isEHFrame)
    {
      //todo: this is based on empirical observation. It is not based
      //on my knowledge of any standard, therefore it is probably
      //wrong in certain circumstances
      currentCIE->addressSize=4;
    }
    else
    {
      currentCIE->addressSize=sizeof(addr_t);
    }
  }
  currentCIE=NULL;
}


lsda_begin_stmt : T_BEGIN T_LSDA_
{
  if(!cfi.exceptTable)
  {
    cfi.exceptTable=zmalloc(sizeof(ExceptTable));
  }
  cfi.exceptTable->numLSDAs++;
  cfi.exceptTable->lsdas=realloc(cfi.exceptTable->lsdas,cfi.exceptTable->numLSDAs*sizeof(LSDA));
  currentLSDA=&cfi.exceptTable->lsdas[cfi.exceptTable->numLSDAs-1];
  memset(currentLSDA,0,sizeof(LSDA));
}

lsda_section : lsda_begin_stmt lsda_part_list T_END T_LSDA_
{
  currentLSDA=NULL;
}

call_site_begin_stmt : T_BEGIN T_CALL_SITE
{
  currentLSDA->numCallSites++;
  currentLSDA->callSiteTable=realloc(currentLSDA->callSiteTable,currentLSDA->numCallSites*sizeof(CallSiteRecord));
  currentCallSite=currentLSDA->callSiteTable+currentLSDA->numCallSites-1;
  memset(currentCallSite,0,sizeof(CallSiteRecord));
}

call_site_section : call_site_begin_stmt call_site_property_stmt_list T_END T_CALL_SITE
{
  currentCallSite=NULL;
}

action_begin_stmt : T_BEGIN T_ACTION
{
  currentLSDA->numActionEntries++;
  currentLSDA->actionTable=realloc(currentLSDA->actionTable,currentLSDA->numActionEntries*sizeof(ActionRecord));
  currentAction=currentLSDA->actionTable+currentLSDA->numActionEntries-1;
}

action_section : action_begin_stmt action_property_stmt_list T_END T_ACTION
{
  currentAction=NULL;
}

top_property_stmt_list : top_property_stmt_list top_property_stmt
{}
| /*empty*/ {}

fde_property_stmt_list : fde_property_stmt_list fde_property_stmt
{}
| /*empty*/ {}

cie_property_stmt_list : cie_property_stmt_list cie_property_stmt
{}
| /*empty*/ {}

instruction_stmt_list : instruction_stmt_list instruction_stmt
{
  RegInstruction** instructions;
  int* numInstrs;
  if(currentCIE)
  {
    instructions=&currentCIE->initialInstructions;
    numInstrs=&currentCIE->numInitialInstructions;
  }
  else
  {
    assert(currentFDE);
    instructions=&currentFDE->instructions;
    numInstrs=&currentFDE->numInstructions;
  }
  *instructions=realloc(*instructions,((*numInstrs) + 1)*sizeof(RegInstruction));
  memcpy((*instructions)+(*numInstrs),&$2.u.regInstr,sizeof(RegInstruction));
  (*numInstrs)++;
}
| /*empty*/ {}

expr_stmt_list : expr_stmt_list expr_stmt
{
  addToDwarfExpression(&currentExpr,$2.u.opInstr);
}
| /*empty*/
{}

lsda_part_list : lsda_part_list lsda_part
{}
| /*empty*/ {}

call_site_property_stmt_list : call_site_property_stmt_list call_site_property_stmt
{}
| /*empty*/ {}

action_property_stmt_list : action_property_stmt_list action_property_stmt
{}
| /*empty*/ {}


top_property_stmt :
section_type_prop {}
| section_location_prop {}
| eh_hdr_location_prop {}
| except_table_addr_prop {}

cie_property_stmt :
index_prop {}
| length_prop {}
| version_prop {}
| augmentation_prop {}
| fde_ptr_enc_prop {}
| fde_lsda_ptr_enc_prop {}
| personality_ptr_enc_prop {}
| personality_prop {}
| address_size_prop {}
| segment_size_prop {}
| data_align_prop {}
| code_align_prop {}
| return_addr_rule_prop {}


fde_property_stmt :
index_prop {}
| length_prop {}
| cie_index_prop {}
| initial_location_prop {}
| address_range_prop {}
| lsda_idx_prop {}

lsda_part :
lsda_property_stmt {}
| call_site_section {}
| action_section {}

lsda_property_stmt :
lpstart_prop {}
| typeinfo_enc_prop {}
| typeinfo_prop {}

call_site_property_stmt :
position_prop {}
| length_prop {}
| landing_pad_prop {}
| has_action_prop {}
| first_action_prop {}

action_property_stmt :
type_idx_prop {}
| next_prop {}



index_prop : T_INDEX ':' nonneg_int_lit
{
  if(currentCIE)
  {
    if($3.u.intval!=currentCIE->idx)
    {
      fprintf(stderr,"CIEs in dwarfscript must be listed in order of their indices\n");
      YYERROR;
    }
  }
  else
  {
    assert(currentFDE);
    if($3.u.intval!=currentFDE->idx)
    {
      fprintf(stderr,"FDEs in dwarfscript must be listed in order of their indices. Found index %i when expectin %i\n",$3.u.intval,currentFDE->idx);
      YYERROR;
    }
  }
}

length_prop : T_LENGTH ':' nonneg_int_lit
{
  if(currentCallSite)
  {
    currentCallSite->length=$3.u.intval;
  }
  else if(currentCIE)
  {
    //ignoring length for now, it changes too easily and we just
    //recompute it when writing the CIE to binary
  }
  else
  {
    fprintf(stderr,"length property not recognized for this section type\n");
    YYERROR;
  }
}

cie_index_prop : T_CIE_INDEX ':' nonneg_int_lit
{
  assert(currentFDE);
  int idx=$3.u.intval;
  if(idx < cfi.numCIEs)
  {
    currentFDE->cie=&cfi.cies[idx];
  }
  else
  {
    fprintf(stderr,"cie_index specifies an index out of range. There are %i cies.\n",cfi.numCIEs);
    YYERROR;
  }
}

initial_location_prop : T_INITIAL_LOCATION ':' nonneg_int_lit
{
  assert(currentFDE);
  currentFDE->lowpc=$3.u.intval;
  //in case address range was encountered first
  //todo: this doesn't work is user is idiotic and has an initial location or
  //statement twice within the same FDE
  currentFDE->highpc=currentFDE->lowpc+currentFDE->highpc;
}

address_range_prop : T_ADDRESS_RANGE ':' nonneg_int_lit
{
  assert(currentFDE);
  currentFDE->highpc=currentFDE->lowpc+$3.u.intval;
}

version_prop : T_VERSION ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->version=$3.u.intval;
}
augmentation_prop : T_AUGMENTATION ':' string_lit
{
  augmentationString=$3.u.stringval;
}
| T_AUGMENTATION ':' error
{
  fprintf(stderr,"augmentation must be a string\n");
  YYERROR;
}

fde_ptr_enc_prop : T_FDE_PTR_ENC ':' dw_pe_lit
{
  assert(currentCIE);
  currentCIE->fdePointerEncoding=$3.u.intval;
  currentCIE->augmentationFlags|=CAF_FDE_ENC | CAF_DATA_PRESENT;
}

fde_lsda_ptr_enc_prop : T_FDE_LSDA_PTR_ENC ':' dw_pe_lit
{
  assert(currentCIE);
  currentCIE->fdeLSDAPointerEncoding=$3.u.intval;
  currentCIE->augmentationFlags|=CAF_FDE_LSDA | CAF_DATA_PRESENT;
}

personality_ptr_enc_prop : T_PERSONALITY_PTR_ENC ':' dw_pe_lit
{
  assert(currentCIE);
  currentCIE->personalityPointerEncoding=$3.u.intval;
  currentCIE->augmentationFlags|=CAF_PERSONALITY | CAF_DATA_PRESENT;
}

personality_prop : T_PERSONALITY ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->personalityFunction=$3.u.intval;
  currentCIE->augmentationFlags|=CAF_PERSONALITY | CAF_DATA_PRESENT;
}

address_size_prop : T_ADDRESS_SIZE ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->addressSize=$3.u.intval;
}
segment_size_prop : T_SEGMENT_SIZE ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->segmentSize=$3.u.intval;
}
data_align_prop : T_DATA_ALIGN ':' int_lit
{
  assert(currentCIE);
  currentCIE->dataAlign=$3.u.intval;
}
code_align_prop : T_CODE_ALIGN ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->codeAlign=$3.u.intval;
}
return_addr_rule_prop : T_RET_ADDR_RULE ':' nonneg_int_lit
{
  assert(currentCIE);
  currentCIE->returnAddrRuleNum=$3.u.intval;
}
| T_RET_ADDR_RULE ':' error
{
  fprintf(stderr,"return_addr_rule must be a non-negative integer\n");
  YYERROR;
}

section_type_prop : T_SECTION_TYPE ':' string_lit
{
  if(!strcmp($3.u.stringval,".eh_frame") || !strcmp($3.u.stringval,"eh_frame"))
  {
    cfi.isEHFrame=true;
  }
  else if(!strcmp($3.u.stringval,".debug_frame") || !strcmp($3.u.stringval,"debug_frame"))
  {
    cfi.isEHFrame=false;
  }
  else
  {
    fprintf(stderr,"Unexpected section type for dwarfscript, expected \".eh_frame\" or \".debug_frame\"\n");
    YYERROR;
  }
  free($3.u.stringval);
}

section_location_prop : T_SECTION_LOC ':' nonneg_int_lit
{
  cfi.sectionAddress=$3.u.intval;
}

eh_hdr_location_prop : T_EH_HDR_LOC ':' nonneg_int_lit
{
  cfi.ehHdrAddress=$3.u.intval;
}

except_table_addr_prop : T_EXCEPT_TABLE_ADDR ':' nonneg_int_lit
{
  cfi.exceptTableAddress=$3.u.intval;
}

lpstart_prop : T_LPSTART ':' nonneg_int_lit
{
  if(!currentLSDA)
  {
    fprintf(stderr,"lpstart property only applicable in LSDA section\n");
    YYERROR;
  }
  currentLSDA->lpStart=$3.u.intval;
}

typeinfo_enc_prop : T_TYPEINFO_ENC ':' dw_pe_lit
{
  if(!currentLSDA)
  {
    fprintf(stderr,"typeinfo_enc property only applicable in LSDA section\n");
    YYERROR;
  }
  currentLSDA->ttEncoding=$3.u.intval;
}

typeinfo_prop : T_TYPEINFO ':' nonneg_int_lit
{
  if(!currentLSDA)
  {
    fprintf(stderr,"typeinfo property only applicable in LSDA section\n");
    YYERROR;
  }
  currentLSDA->numTypeEntries++;
  currentLSDA->typeTable=realloc(currentLSDA->typeTable,currentLSDA->numTypeEntries*sizeof(word_t));
  currentLSDA->typeTable[currentLSDA->numTypeEntries-1]=$3.u.intval;
}

position_prop : T_POSITION ':' nonneg_int_lit
{
  if(!currentCallSite)
  {
    fprintf(stderr,"position property only applicable in CALL_SITE section\n");
    YYERROR;
  }
  currentCallSite->position=$3.u.intval;
}

landing_pad_prop : T_LANDING_PAD ':' nonneg_int_lit
{
  if(!currentCallSite)
  {
    fprintf(stderr,"landing_pad property only applicable in CALL_SITE section\n");
    YYERROR;
  }
  currentCallSite->landingPadPosition=$3.u.intval;
}

has_action_prop : T_HAS_ACTION ':' bool_lit
{
  if(!currentCallSite)
  {
    fprintf(stderr,"has_action property only applicable in CALL_SITE section\n");
    YYERROR;
  }
  currentCallSite->hasAction=$3.u.boolval;
}


first_action_prop : T_FIRST_ACTION ':' nonneg_int_lit
{
  if(!currentCallSite)
  {
    fprintf(stderr,"first_action property only applicable in CALL_SITE section\n");
    YYERROR;
  }
  currentCallSite->firstAction=$3.u.intval;
}

type_idx_prop : T_TYPE_IDX ':' nonneg_int_lit
{
  if(!currentAction)
  {
    fprintf(stderr,"type_idx property only applicable in ACTION section\n");
    YYERROR;
  }
  currentAction->typeFilterIndex=$3.u.intval;
}

next_prop : T_NEXT ':' nonneg_int_lit
{
  if(!currentAction)
  {
    fprintf(stderr,"'next' property only applicable in ACTION section\n");
    YYERROR;
  }
  currentAction->nextAction=$3.u.intval;
  currentAction->hasNextAction=true;
}
| T_NEXT ':' T_NONE
{
  if(!currentAction)
  {
    fprintf(stderr,"'next' property only applicable in ACTION section\n");
    YYERROR;
  }
  currentAction->hasNextAction=false;
}

lsda_idx_prop : T_LSDA_IDX ':' nonneg_int_lit
{
  assert(currentFDE);
  currentFDE->hasLSDAPointer=true;
  currentFDE->lsdaIdx=$3.u.intval;
}


////////////////////////////////
//literals
///////////////////////////////

int_lit : T_INT
{
  $$.u.intval=savedInt;
}
| T_POS_INT
{
  $$.u.intval=savedInt;
}

register_lit : T_REGISTER
{
  $$.u.intval=savedRegisterNumber;
}

nonneg_int_lit : T_NONNEG_INT
{
  $$.u.intval=savedInt;
}
| T_POS_INT
{
  $$.u.intval=savedInt;
}

string_lit : T_STRING_LITERAL
{
  $$.u.stringval=savedString;
}

/*hexdata lit no longer used now that we don't allow direct hex setting of augmentation data
  data_lit : T_HEXDATA
{
  $$.u.dataval.data=savedData;
  $$.u.dataval.len=savedDataLen;
}
*/

bool_lit : T_BOOL_TRUE
{
  $$.u.boolval=true;
}
| T_BOOL_FALSE
{
  $$.u.boolval=false;
}

dw_pe_lit : T_DW_PTR_ENCODING
{
  $$.u.intval=savedInt;
}
| dw_pe_lit ',' dw_pe_lit
{
  $$.u.intval=$1.u.intval | $3.u.intval;
}




//OK, here come the DWARF instructions. There are a lot of them

instruction_stmt : dw_cfa_set_loc {$$=$1;}
|dw_cfa_advance_loc {$$=$1;}
|dw_cfa_advance_loc1 {$$=$1;}
|dw_cfa_advance_loc2 {$$=$1;}
|dw_cfa_advance_loc4 {$$=$1;}
|dw_cfa_offset {$$=$1;}
|dw_cfa_offset_extended {$$=$1;}
|dw_cfa_offset_extended_sf {$$=$1;}
|dw_cfa_restore {$$=$1;}
|dw_cfa_restore_extended {$$=$1;}
|dw_cfa_nop {$$=$1;}
|dw_cfa_undefined {$$=$1;}
|dw_cfa_same_value {$$=$1;}
|dw_cfa_register {$$=$1;}
|dw_cfa_remember_state {$$=$1;}
|dw_cfa_restore_state {$$=$1;}
|dw_cfa_def_cfa {$$=$1;}
|dw_cfa_def_cfa_sf {$$=$1;}
|dw_cfa_def_cfa_register {$$=$1;}
|dw_cfa_def_cfa_offset {$$=$1;}
|dw_cfa_def_cfa_offset_sf {$$=$1;}
|dw_cfa_def_cfa_expression {$$=$1;}
|dw_cfa_expression {$$=$1;}
|dw_cfa_val_offset {$$=$1;}
|dw_cfa_val_offset_sf {$$=$1;}
|dw_cfa_val_expression {$$=$1;}


dw_cfa_set_loc : T_DW_CFA_set_loc nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_set_loc;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_advance_loc : T_DW_CFA_advance_loc nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_advance_loc;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_advance_loc1 : T_DW_CFA_advance_loc1 nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_advance_loc1;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_advance_loc2 : T_DW_CFA_advance_loc2 nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_advance_loc2;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_advance_loc4 : T_DW_CFA_advance_loc4 nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_advance_loc4;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_offset : T_DW_CFA_offset register_lit nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_offset;
  makeBasicRegister1(&$$,&$2);
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_offset_extended : T_DW_CFA_offset_extended register_lit nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_offset_extended;
  makeBasicRegister1(&$$,&$2);
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_offset_extended_sf : T_DW_CFA_offset_extended_sf register_lit int_lit
{
  $$.u.regInstr.type=DW_CFA_offset_extended_sf;
  makeBasicRegister1(&$$,&$2);
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_val_offset : T_DW_CFA_val_offset register_lit nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_val_offset;
  makeBasicRegister1(&$$,&$2);
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_val_offset_sf : T_DW_CFA_val_offset_sf register_lit int_lit
{
  $$.u.regInstr.type=DW_CFA_val_offset_sf;
  makeBasicRegister1(&$$,&$2);
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_restore : T_DW_CFA_restore register_lit
{
  $$.u.regInstr.type=DW_CFA_restore;
  makeBasicRegister1(&$$,&$2);
}
dw_cfa_restore_extended : T_DW_CFA_restore_extended register_lit
{
  $$.u.regInstr.type=DW_CFA_restore_extended;
  makeBasicRegister1(&$$,&$2);
}
dw_cfa_nop : T_DW_CFA_nop
{
  $$.u.regInstr.type=DW_CFA_nop;
}
dw_cfa_undefined : T_DW_CFA_undefined register_lit
{
  $$.u.regInstr.type=DW_CFA_undefined;
  makeBasicRegister1(&$$,&$2);
}
dw_cfa_same_value : T_DW_CFA_same_value register_lit
{
  $$.u.regInstr.type=DW_CFA_same_value;
  makeBasicRegister1(&$$,&$2);
}
dw_cfa_register : T_DW_CFA_register register_lit register_lit
{
  $$.u.regInstr.type=DW_CFA_register;
  makeBasicRegister1(&$$,&$2);
  makeBasicRegister2(&$$,&$3);
}
dw_cfa_remember_state : T_DW_CFA_remember_state
{
  $$.u.regInstr.type=DW_CFA_remember_state;
}
dw_cfa_restore_state : T_DW_CFA_restore_state
{
  $$.u.regInstr.type=DW_CFA_restore_state;
}
dw_cfa_def_cfa : T_DW_CFA_def_cfa register_lit nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_def_cfa;
  makeBasicRegister1(&$$,&$2);
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_def_cfa_sf : T_DW_CFA_def_cfa_sf register_lit int_lit
{
  $$.u.regInstr.type=DW_CFA_def_cfa;
  makeBasicRegister1(&$$,&$2);
  
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg2=$3.u.intval;
}
dw_cfa_def_cfa_register : T_DW_CFA_def_cfa_register register_lit
{
  $$.u.regInstr.type=DW_CFA_def_cfa_register;
  makeBasicRegister1(&$$,&$2);
}
dw_cfa_def_cfa_offset : T_DW_CFA_def_cfa_offset nonneg_int_lit
{
  $$.u.regInstr.type=DW_CFA_def_cfa_offset;
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_def_cfa_offset_sf : T_DW_CFA_def_cfa_offset_sf  int_lit
{
  $$.u.regInstr.type=DW_CFA_def_cfa;
  //todo: include dataAlign from CIE here or not? parseFDEInstructions
  //currently does. The problem is that's harder to do here. The
  //problem then becomes that we are doing different things here than
  //in parseFDEInstructions and one of them must be wrong with regard
  //to how we're printing things out and emitting them. I need to
  //think about which one actually makes more sense
  $$.u.regInstr.arg1=$2.u.intval;
}
dw_cfa_def_cfa_expression : T_DW_CFA_def_cfa_expression  expression_section
{
  $$.u.regInstr.type=DW_CFA_def_cfa_expression;
  $$.u.regInstr.expr=$2.u.expr;
}
dw_cfa_expression : T_DW_CFA_expression register_lit expression_section
{
  $$.u.regInstr.type=DW_CFA_expression;
  makeBasicRegister1(&$$,&$2);
  $$.u.regInstr.expr=$3.u.expr;
}
dw_cfa_val_expression : T_DW_CFA_val_expression register_lit expression_section
{
  $$.u.regInstr.type=DW_CFA_val_expression;
  makeBasicRegister1(&$$,&$2);
  $$.u.regInstr.expr=$3.u.expr;
}


//DWARF expression instructions


expr_stmt :
dw_op_addr {$$=$1;}
|dw_op_deref {$$=$1;}
|dw_op_const1u {$$=$1;}
|dw_op_const1s {$$=$1;}
|dw_op_const2u {$$=$1;}
|dw_op_const2s {$$=$1;}
|dw_op_const4u {$$=$1;}
|dw_op_const4s {$$=$1;}
|dw_op_const8u {$$=$1;}
|dw_op_const8s {$$=$1;}
|dw_op_constu {$$=$1;}
|dw_op_consts {$$=$1;}
|dw_op_dup {$$=$1;}
|dw_op_drop {$$=$1;}
|dw_op_over {$$=$1;}
|dw_op_pick {$$=$1;}
|dw_op_swap {$$=$1;}
|dw_op_rot {$$=$1;}
|dw_op_xderef {$$=$1;}
|dw_op_abs {$$=$1;}
|dw_op_and {$$=$1;}
|dw_op_div {$$=$1;}
|dw_op_minus {$$=$1;}
|dw_op_mod {$$=$1;}
|dw_op_mul {$$=$1;}
|dw_op_neg {$$=$1;}
|dw_op_not {$$=$1;}
|dw_op_or {$$=$1;}
|dw_op_plus {$$=$1;}
|dw_op_plus_uconst {$$=$1;}
|dw_op_shl {$$=$1;}
|dw_op_shr {$$=$1;}
|dw_op_shra {$$=$1;}
|dw_op_xor {$$=$1;}
|dw_op_skip {$$=$1;}
|dw_op_bra {$$=$1;}
|dw_op_eq {$$=$1;}
|dw_op_ge {$$=$1;}
|dw_op_gt {$$=$1;}
|dw_op_le {$$=$1;}
|dw_op_lt {$$=$1;}
|dw_op_ne {$$=$1;}
|dw_op_litn {$$=$1;}
|dw_op_regn {$$=$1;}
|dw_op_bregn {$$=$1;}
|dw_op_regx {$$=$1;}
|dw_op_fbreg {$$=$1;}
|dw_op_bregx {$$=$1;}
|dw_op_piece {$$=$1;}
|dw_op_deref_size {$$=$1;}
|dw_op_xderef_size {$$=$1;}
|dw_op_nop {$$=$1;}
|dw_op_push_object_address {$$=$1;}
|dw_op_call2 {$$=$1;}
|dw_op_call4 {$$=$1;}
|dw_op_call_ref {$$=$1;}
|dw_op_form_tls_address {$$=$1;}
|dw_op_call_frame_cfa {$$=$1;}
|dw_op_bit_piece {$$=$1;}
|dw_op_implicit_value {$$=$1;}
|dw_op_stack_value {$$=$1;}

dw_op_addr : T_DW_OP_addr nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_addr;
  $$.u.opInstr.arg1=$2.u.intval;
}

dw_op_deref : T_DW_OP_deref
{
  $$.u.opInstr.type=DW_OP_deref;
}

dw_op_const1u : T_DW_OP_const1u nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_const1u;
  $$.u.opInstr.arg1=$2.u.intval;
}

dw_op_const1s : T_DW_OP_const1s int_lit
{
  $$.u.opInstr.type=DW_OP_const1s;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const2u : T_DW_OP_const2u nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_const2u;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const2s : T_DW_OP_const2s int_lit
{
  $$.u.opInstr.type=DW_OP_const2s;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const4u : T_DW_OP_const4u nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_const4u;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const4s : T_DW_OP_const4s int_lit
{
  $$.u.opInstr.type=DW_OP_const4s;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const8u : T_DW_OP_const8u nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_const8u;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_const8s : T_DW_OP_const8s int_lit
{
  $$.u.opInstr.type=DW_OP_const8s;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_constu : T_DW_OP_constu nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_constu;
  $$.u.opInstr.arg1=$2.u.intval;
}
| T_DW_OP_constu
{
  fprintf(stderr,"expected integer constant after DW_OP_constu\n");
  YYERROR;
}

dw_op_consts : T_DW_OP_consts int_lit
{
  $$.u.opInstr.type=DW_OP_consts;
  $$.u.opInstr.arg1=$2.u.intval;
}

dw_op_dup : T_DW_OP_dup
{
  $$.u.opInstr.type=DW_OP_dup
}
dw_op_drop : T_DW_OP_drop
{
$$.u.opInstr.type=DW_OP_drop
}
dw_op_over : T_DW_OP_over
{
$$.u.opInstr.type=DW_OP_over
}
dw_op_pick : T_DW_OP_pick nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_pick;
  $$.u.opInstr.arg1=savedInt;
}
dw_op_swap : T_DW_OP_swap
{
$$.u.opInstr.type=DW_OP_swap
}
dw_op_rot : T_DW_OP_rot
{
$$.u.opInstr.type=DW_OP_rot
}
dw_op_xderef : T_DW_OP_xderef
{
$$.u.opInstr.type=DW_OP_xderef
}
dw_op_abs : T_DW_OP_abs
{
  $$.u.opInstr.type=DW_OP_abs;
}
dw_op_and : T_DW_OP_and
{
  $$.u.opInstr.type=DW_OP_and;
}
dw_op_div : T_DW_OP_div
{
  $$.u.opInstr.type=DW_OP_div;
}
dw_op_minus : T_DW_OP_minus
{
  $$.u.opInstr.type=DW_OP_minus;
}
dw_op_mod : T_DW_OP_mod
{
  $$.u.opInstr.type=DW_OP_mod;
}
dw_op_mul : T_DW_OP_mul
{
  $$.u.opInstr.type=DW_OP_mul;
}
dw_op_neg : T_DW_OP_neg
{
  $$.u.opInstr.type=DW_OP_neg;
}
dw_op_not : T_DW_OP_not
{
  $$.u.opInstr.type=DW_OP_not;
}
dw_op_or : T_DW_OP_or
{
  $$.u.opInstr.type=DW_OP_or;
}
dw_op_plus : T_DW_OP_plus
{
  $$.u.opInstr.type=DW_OP_plus;
}
dw_op_plus_uconst : T_DW_OP_plus_uconst
{
  $$.u.opInstr.type=DW_OP_plus_uconst;
}
dw_op_shl : T_DW_OP_shl
{
  $$.u.opInstr.type=DW_OP_shl;
}
dw_op_shr : T_DW_OP_shr
{
  $$.u.opInstr.type=DW_OP_shr;
}
dw_op_shra : T_DW_OP_shra
{
  $$.u.opInstr.type=DW_OP_shra;
}
dw_op_xor : T_DW_OP_xor
{
  $$.u.opInstr.type=DW_OP_xor;
}
dw_op_skip : T_DW_OP_skip int_lit
{
  $$.u.opInstr.type=DW_OP_skip;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_bra : T_DW_OP_bra int_lit
{
  $$.u.opInstr.type=DW_OP_bra;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_eq : T_DW_OP_eq
{
  $$.u.opInstr.type=DW_OP_eq;
}
dw_op_ge : T_DW_OP_ge
{
  $$.u.opInstr.type=DW_OP_ge;
}
dw_op_gt : T_DW_OP_gt
{
  $$.u.opInstr.type=DW_OP_gt;
}
dw_op_le : T_DW_OP_le
{
  $$.u.opInstr.type=DW_OP_le;
}
dw_op_lt : T_DW_OP_lt
{
  $$.u.opInstr.type=DW_OP_lt;
}
dw_op_ne : T_DW_OP_ne
{
  $$.u.opInstr.type=DW_OP_ne;
}
dw_op_litn : T_DW_OP_litn
{
  if(savedInt > 31)
  {
    logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_lit cannot take value greater than 31\n");
    YYERROR;
  }
  $$.u.opInstr.type=DW_OP_lit0+savedInt;
}
dw_op_regn : T_DW_OP_regn nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_reg0+$2.u.intval;
}

//stage because we have to get savedInt here while it's valid
stage_dw_op_bregn : T_DW_OP_bregn
{
  $$.u.opInstr.type=DW_OP_breg0+savedInt;
}

dw_op_bregn : stage_dw_op_bregn int_lit
{
  $$.u.opInstr.type=$1.u.opInstr.type;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_regx : T_DW_OP_regx nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_regx;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_fbreg : T_DW_OP_fbreg 
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_fbreg not implemented yet\n");
  YYERROR;
}
dw_op_bregx : T_DW_OP_bregx nonneg_int_lit int_lit
{
  $$.u.opInstr.type=DW_OP_bregx;
  $$.u.opInstr.arg1=$2.u.intval;
  $$.u.opInstr.arg2=$3.u.intval;
}
dw_op_piece : T_DW_OP_piece
{
  $$.u.opInstr.type=DW_OP_piece
  
}
dw_op_deref_size : T_DW_OP_deref_size nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_deref_size;
  $$.u.opInstr.arg1=$2.u.intval;
  if($$.u.opInstr.arg1 > 255)
  {
    logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_deref_size operand out of range\n");
    YYERROR;
  }
}

dw_op_xderef_size : T_DW_OP_xderef_size nonneg_int_lit
{
  $$.u.opInstr.type=DW_OP_xderef_size;
  $$.u.opInstr.arg1=$2.u.intval;
}
dw_op_nop : T_DW_OP_nop
{
$$.u.opInstr.type=DW_OP_nop
}
dw_op_push_object_address : T_DW_OP_push_object_address
{
$$.u.opInstr.type=DW_OP_push_object_address
}
dw_op_call2 : T_DW_OP_call2
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_call2 not implemented yet\n");
  YYERROR;
}
dw_op_call4 : T_DW_OP_call4
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_call4 not implemented yet\n");
  YYERROR;
}
dw_op_call_ref : T_DW_OP_call_ref
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_OP_call_ref not implemented yet\n");
  YYERROR;
}
dw_op_form_tls_address : T_DW_OP_form_tls_address
{
$$.u.opInstr.type=DW_OP_form_tls_address
}
dw_op_call_frame_cfa : T_DW_OP_call_frame_cfa
{
$$.u.opInstr.type=DW_OP_call_frame_cfa
}
dw_op_bit_piece : T_DW_OP_bit_piece
{
$$.u.opInstr.type=DW_OP_bit_piece
}
dw_op_implicit_value : T_DW_OP_implicit_value
{
$$.u.opInstr.type=DW_OP_implicit_value
}
dw_op_stack_value : T_DW_OP_stack_value
{
$$.u.opInstr.type=DW_OP_stack_value
}

%%

void makeBasicRegister1(ParseNode* node,ParseNode* intvalNode)
{
  node->u.regInstr.arg1=intvalNode->u.intval;
  node->u.regInstr.arg1Reg.type=ERT_BASIC;
  node->u.regInstr.arg1Reg.u.index=intvalNode->u.intval;
}
void makeBasicRegister2(ParseNode* node,ParseNode* intvalNode)
{
  node->u.regInstr.arg2=intvalNode->u.intval;
  node->u.regInstr.arg2Reg.type=ERT_BASIC;
  node->u.regInstr.arg2Reg.u.index=intvalNode->u.intval;
}
int yyerror(char *s)
{
  fprintf(stderr, "dwarfscript %s at line %d\n", s, dwLineNumber);
  return 0;
}
