/*
  File: types.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: utility functions for dealing with the types defined in types.h
*/

#include "types.h"


//I wish C had lambda functions
void releaseRefCountedType(TypeInfo* ti)
{
  releaseRefCounted((RefCounted*)ti,(FreeFunc)freeTypeInfo);
}

void freeTypeAndVarInfo(TypeAndVarInfo* tv)
{
  if(0==dictRelease(tv->globalVars))
  {
    dictDelete(tv->globalVars,freeVarInfoVoid);
  }
  
  dictDelete(tv->types,(FreeFunc)releaseRefCountedType);
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
    

void freeSubprogramInfo(SubprogramInfo* si)
{
  free(si->name);
  deleteList(si->typesHead,NULL);
  free(si);
}

void freeCompilationUnit(CompilationUnit* cu)
{
  freeTypeAndVarInfo(cu->tv);
  free(cu->name);
  free(cu->id);
  cu->name=(void*)0xbadf00d;
  cu->tv=(void*)0xbadf00d;
  dictDelete(cu->subprograms,(FreeFunc)freeSubprogramInfo);
  free(cu);
}

void freeDwarfInfo(DwarfInfo* di)
{
  List* next=di->compilationUnits;
  while(next)
  {
    List* next_=next->next;
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
      releaseRefCounted((RC*)t->fieldTypes[i],(FreeFunc)freeTypeInfo);
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
  if(t->transformer)
  {
    freeTypeTransform(t->transformer);
    t->transformer=(void*)0xbaadf00d;
  }
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
  free(t->fieldTransformTypes);
  free(t);
}

