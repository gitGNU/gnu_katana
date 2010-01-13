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

#include "dictionary.h"

typedef unsigned int uint;
typedef unsigned long addr_t;
typedef uint word_t;

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
  Dictionary* globalTypes; //maps type names to TypeInfo structs
  Dictionary* globalVars;  //maps var names to VarInfo structs
} TypeAndVarInfo;

//struct to hold information about a struct in the target program
typedef struct
{
  char* name;
  int length;//overall length in bytes of the struct
  int numFields;
  char** fields;
  int* fieldLengths;
  char** fieldTypes;
} TypeInfo;

#define FIELD_DELETED -2

//holds information on how to transform one type to another
typedef struct
{
  TypeInfo* from;
  TypeInfo* to;
  //for each field in the 'from' typeinfo,
  //contains the offset of that field from the start of the structure
  //this can be used to relocate fields within structures
  //the value FIELD_DELETED indicates that the field is no longer
  //present in the structure
  int* fieldOffsets;
  //todo: support fixups of field types within structures
} TypeTransform;

typedef struct
{
  char* name;
  TypeInfo* type;
  //location can be gotten from the symbol table
} VarInfo;

//structure to hold information about transformation
//necessary to hotpatch types
typedef struct
{
  Dictionary* typeTransformers;
  List* varsToTransform;
  //the following two variables are so that
  //we can mmap in page-sized chunks of space
  //at a time and use them for multiple variables
  //they are set at the hot-patching stage
  //rather than at the patch calculation stage
  long addrFreeSpace;
  uint freeSpaceLeft;
} TransformationInfo;

#endif
