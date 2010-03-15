/*
  File: types.h
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: types for keeping track of types :)
               Also some function prototypes for comparing
               types and whatnot
*/
#ifndef types_h
#define types_h

#include "util/dictionary.h"
#include "util/map.h"
#include "util/list.h"
#include <libdwarf.h>
#include <string.h>
#include "util/refcounted.h"

typedef unsigned int uint;
typedef size_t addr_t;
typedef size_t word_t;
typedef short unsigned int usint;
typedef size_t idx_t;

//we keep track of the type information
//from on ELF file in a TypesInfo struct
//note that at the moment we're only worrying about structures
//because they're the only sort of types that can change
//in real world are other things to take into account
//(like the addition and removal of variables, a variable changing its type, etc)
typedef struct
{
  //todo: in the future, may separate things out
  //by compilation unit
  //only dealing with global variable for now
  Dictionary* types; //maps type names to TypeInfo structs
  //List* globalTypesList;//exists to give a unique listing of types, as the dictionary contains typedefs, etc //todo: support this
  Dictionary* globalVars;  /*maps var names to VarInfo structs.  */
  Map* parsedDies; //contains keys that areglobal offsets of dwarf
                   //dies we've parsed so far this is necessary
                   //because we don't necessarily parse them in order
                   //because a die can refer to a die that comes
                   //later. The values are the data 

                       
} TypeAndVarInfo;

void freeTypeAndVarInfo(TypeAndVarInfo* tv);

typedef struct
{
  TypeAndVarInfo* tv;
  Dictionary* subprograms;//functions in this compilation unit mapped by name. Type of SubprogramInfo
  char* name;
  char* id;//in case two compilation units have the same name
  bool presentInOtherVersion;
  Dwarf_P_Die die;//for when writing out a patch
  Dwarf_P_Die lastDie;//for when writing out a patch
  struct ElfInfo* elf;//elf file this comilation unit is in
} CompilationUnit;

void freeCompilationUnit(CompilationUnit* cu);

typedef struct
{
  char* name;
  addr_t lowpc;
  addr_t highpc;
  List* typesHead;//list of types used within the subprogram. Later
                    //we can just look through these types and if any
                    //of them has a transformer then it can't have an
                    //activation frame during patching
  List* typesTail;
  CompilationUnit* cu;
} SubprogramInfo;


typedef struct
{
  List* compilationUnits;
  List* lastCompilationUnit;
} DwarfInfo;

void freeDwarfInfo(DwarfInfo* di);

typedef enum
{
  TT_STRUCT=1,
  TT_BASE,
  TT_POINTER,
  TT_ARRAY,
  TT_UNION,
  TT_ENUM,
  TT_SUBROUTINE_TYPE,//for function pointers to point to
  TT_CONST,//const version of another type
  TT_VOID //the void type
} TYPE_TYPE;

struct TypeTransform_;//forward declare

//struct to hold information about a type in the target program
//not all members are used by all types (for example, structs use more)
typedef struct TypeInfo_
{
  RefCounted rc;//note that when calling the grab and release methods
                //we should pass a pointer to the whole type,
                //cast. This will work properly because of the way C
                //lays out memory as long as rc remains the first
                //member of TypeInfo
  char* name;
  TYPE_TYPE type;
  int length;//overall length in bytes of the type
  bool incomplete;/*if true indicates that this type is in the process
                    of being read. Necessary if for instance a struct
                    has a field which is a pointer to another of the
                    same struct, reading the struct members will
                    reference this type which is only half read
                    itself*/
  bool declaration;//this type only represents a declaration, and is
                   //not a full definition
  CompilationUnit* cu; //which compilation unit the type is in. NULL
                       //if the type is visible to all compilation
                       //units
  Dwarf_P_Die die;//used when writing patch info to disk
  struct TypeTransform_* transformer;//how to transform the type into its other form
  uint fde;//identifier (offset) for fde containing info on how to transform this type
  ///////////////////////////////////////////
  //only applicable to structs, unions, and
  //subroutineTypes. subroutineTypes do not set
  //fieldLengths, but do use fieldTypes
  //to keep track of the types to be passed
  //to the subroutine
  ///////////////////////////////////////////
  int numFields;
  char** fields;
  int* fieldLengths;
  struct TypeInfo_** fieldTypes;
  ///////////////////////////////////////////
  //end only applicable to structs and unions
  ///////////////////////////////////////////
  //applicable only to pointers, arrays, or const
  ///////////////////////////////////////////
  struct TypeInfo_* pointedType;//could also be called arrayType
  ///////////////////////////////////////////
  //end applicable only to pointers, arrays, or const
  ///////////////////////////////////////////
  //applicable only to arrays
  ///////////////////////////////////////////
  int lowerBound;
  int upperBound;
  ///////////////////////////////////////////
  //end applicable only to arrays
  ///////////////////////////////////////////
} TypeInfo;

TypeInfo* duplicateTypeInfo(const TypeInfo* t);
void freeTypeInfo(TypeInfo* t);

#define FIELD_DELETED -2
typedef enum
{
  EFTT_DELETE=FIELD_DELETED,
  EFTT_COPY=1,
  EFTT_RECURSE
}E_FIELD_TRANSFORM_TYPE;

//holds information on how to transform one type to another
typedef struct TypeTransform_
{
  TypeInfo* from;
  TypeInfo* to;
  bool straightCopy;//just copy byte for byte, ignore any structure. If true, all other fields besides from are ignored
  //for each field in the 'from' typeinfo,
  //contains the new offset of that field from the start of the new structure
  //this can be used to relocate fields within structures
  //the value FIELD_DELETED indicates that the field is no longer
  //present in the structure.
  int* fieldOffsets;
  E_FIELD_TRANSFORM_TYPE* fieldTransformTypes;//normal copy, field deleted, or recurse
  bool onDisk;//used when writing patch info to disk
  idx_t fdeIdx;//index of FDE corresponding to the transformation
} TypeTransform;

void freeTypeTransform(TypeTransform* t);

typedef struct
{
  char* name;
  TypeInfo* type;
  addr_t newLocation;//used in applying the patch
  addr_t oldLocation;//used in applying the patch
  bool declaration;//true for example for a variable declared extern.
                   //The declaration copy is discarded if a real copy is found
} VarInfo;


void freeVarInfo(VarInfo* v);

//wrapper
void freeVarInfoVoid(void* v);


//todo: is this type necessary, I think we could
//get at all of this thorugh var->type
typedef struct
{
  VarInfo* var;
  TypeTransform* transform;
  CompilationUnit* cu;
} VarTransformation;

#endif
