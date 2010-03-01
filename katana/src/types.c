/*
  File: types.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: utility functions for dealing with the types defined in types.h
*/

#include "types.h"




void freeTypeAndVarInfo(TypeAndVarInfo* tv)
{
  if(0==dictRelease(tv->globalVars))
  {
    dictDelete(tv->globalVars,freeVarInfoVoid);
  }
  //todo: keep track of unique types so can free them
  //can't free them through the dictionary because we have typedefs
  dictDelete(tv->types,NULL);
  mapDelete(tv->parsedDies,&free,NULL);
  free(tv);
}


void freeVarInfo(VarInfo* v)
{
  free(v->name);
  v->name=(void*)0xbadf00d;
  //todo: should types be ref counted
  free(v);
}


//wrapper
void freeVarInfoVoid(void* v)
{
  freeVarInfo((VarInfo*)v);
}
    

void freeCompilationUnit(CompilationUnit* cu)
{
  freeTypeAndVarInfo(cu->tv);
  free(cu->name);
  free(cu->id);
  cu->name=(void*)0xbadf00d;
  cu->tv=(void*)0xbadf00d;
  free(cu);
}

void freeDwarfInfo(DwarfInfo* di)
{
  List* next=di->compilationUnits;
  while(next)
  {
    List* next_=next;
    freeCompilationUnit(next->value);
    next->value=(void*)0xbadf00d;
    free(next);
    next=next_;
  }
  di->compilationUnits=(void*)0xbadf00d;
  di->lastCompilationUnit=(void*)0xbadf00d;
  free(di);
}

void freeTypeInfo(TypeInfo* t)
{
  free(t->name);
  for(int i=0;i<t->numFields;i++)
  {
    if(t->fields[i])
    {
      free(t->fields[i]);
    }
    if(t->fieldTypes[i])
    {
      free(t->fieldTypes[i]);
    }
    t->fields[i]=(void*)0xbadf00d;
    t->fieldTypes[i]=(void*)0xbadf00d;
  }
  free(t->fields);
  free(t->fieldLengths);
  free(t->fieldTypes);
  t->fieldTypes=(void*)0xbadf00d;
  t->fields=(void*)0xbadf00d;
  t->fieldLengths=(void*)0xbadf00d;
  free(t);
}

//wrapper
void freeTypeTransformVoid(void* t)
{
  freeTypeTransform((TypeTransform*)t);
}

void freeTypeTransform(TypeTransform* t)
{
  //todo: refcount types?
  free(t->fieldOffsets);
  free(t);
}

