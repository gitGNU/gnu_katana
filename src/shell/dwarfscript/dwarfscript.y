%{
#include "parse_helper.h"
#include "callFrameInfo.h"
#include "util/logging.h"
CallFrameInfo cfi;
CallFrameInfo* parsedCallFrameInfo=&cfi; 
FDE* currentFDE;
CIE* currentCIE; 
 
#define YYSTYPE ParseNode
#define YYDEBUG 1

extern int yydwlex();

int dwLineNumber;
 //values saved by the lexer
char* savedString;
int savedRegisterNumber;
int savedInt;
double savedDouble;
int savedDataLen;
byte* savedData; 

int yyerror(char *s);
void makeBasicRegister1(ParseNode* node,ParseNode* intvalNode);
void makeBasicRegister2(ParseNode* node,ParseNode* intvalNode);

 %}

//different prefix to avoid conflicting with our other parser
%name-prefix "yydw"

//token definitions
%token T_BEGIN T_END
%token T_INDEX T_DATA_ALIGN T_CODE_ALIGN T_RET_ADDR_RULE T_AUGMENTATION
%token T_INITIAL_LOCATION T_ADDRESS_RANGE T_OFFSET T_LENGTH T_CIE_INDEX
%token T_VERSION T_ADDRESS_SIZE T_SEGMENT_SIZE T_STRING_LITERAL T_HEXDATA
%token T_AUGMENTATION_DATA
%token T_FDE T_CIE T_INSTRUCTIONS
%token T_POS_INT T_NONNEG_INT T_INT T_FLOAT T_REGISTER T_INVALID_TOKEN
%token T_DW_CFA_advance_loc T_DW_CFA_offset T_DW_CFA_restore T_DW_CFA_extended
%token T_DW_CFA_nop T_DW_CFA_set_loc T_DW_CFA_advance_loc1 T_DW_CFA_advance_loc2
%token T_DW_CFA_advance_loc4 T_DW_CFA_offset_extended T_DW_CFA_restore_extended
%token T_DW_CFA_undefined T_DW_CFA_same_value T_DW_CFA_register
%token T_DW_CFA_remember_state T_DW_CFA_restore_state T_DW_CFA_def_cfa
%token T_DW_CFA_def_cfa_register T_DW_CFA_def_cfa_offset
%token T_DW_CFA_def_cfa_expression T_DW_CFA_expression T_DW_CFA_offset_extended_sf
%token T_DW_CFA_def_cfa_sf  T_DW_CFA_def_cfa_offset_sf T_DW_CFA_val_offset
%token T_DW_CFA_val_offset_sf T_DW_CFA_val_expression

%%

section_list : section_list fde_section
{}
| section_list cie_section {}
| /*empty*/ {}

fde_section : fde_begin_stmt fde_property_stmt_list instruction_section fde_property_stmt_list fde_end_stmt
{}

cie_section : cie_begin_stmt cie_property_stmt_list instruction_section cie_property_stmt_list cie_end_stmt
{}

instruction_section : T_BEGIN T_INSTRUCTIONS instruction_stmt_list T_END T_INSTRUCTIONS
{
  $$=$3;
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
  //set the address size in case it's not specified in the dwarfscript we're parsing
  currentCIE->addressSize=sizeof(addr_t);
  cfi.numCIEs++;
}

cie_end_stmt : T_END T_CIE
{
  currentCIE=NULL;
}

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

cie_property_stmt :
index_prop {}
| length_prop {}
| version_prop {}
| augmentation_prop {}
| augmentation_data_prop {}
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
| augmentation_data_prop {}

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
  //ignoring length for now, it changes too easily and we just
  //recompute it when writing the CIE to binary
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
  assert(currentCIE);
  currentCIE->augmentation=$3.u.stringval;
}
| T_AUGMENTATION ':' error
{
  fprintf(stderr,"augmentation must be a string\n");
  YYERROR;
}

augmentation_data_prop : T_AUGMENTATION_DATA ':' data_lit
{
  if(currentCIE)
  {
    currentCIE->augmentationData=$3.u.dataval.data;
    currentCIE->augmentationDataLen=$3.u.dataval.len;
  }
  else
  {
    assert(currentFDE);
    currentFDE->augmentationData=$3.u.dataval.data;
    currentFDE->augmentationDataLen=$3.u.dataval.len;
  }
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
code_align_prop : T_CODE_ALIGN ':' int_lit
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

int_lit : T_INT
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

data_lit : T_HEXDATA
{
  $$.u.dataval.data=savedData;
  $$.u.dataval.len=savedDataLen;
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
dw_cfa_def_cfa_expression : T_DW_CFA_def_cfa_expression
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_CFA_def_cfa_expression not implemented yet\n");
  YYERROR;
}
dw_cfa_expression : T_DW_CFA_expression
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_CFA_expression not implemented yet\n");
  YYERROR;
}
dw_cfa_val_expression : T_DW_CFA_val_expression
{
  logprintf(ELL_ERR,ELS_DWARFSCRIPT,"DW_CFA_val_expression not implemented yet\n");
  YYERROR;
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
