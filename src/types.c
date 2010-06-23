/*
  File: types.c
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

  //todo: should free keys but valgrind's showing
  //an error on it, and I'm too lazy to debug it fully
  mapDelete(tv->parsedDies,NULL,NULL);
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
  free(t->fieldOffsets);
  free(t->fieldTypes);
  t->fieldTypes=(void*)0xbadf00d;
  t->fields=(void*)0xbadf00d;
  t->fieldOffsets=(void*)0xbadf00d;
  if(t->lowerBounds)
  {
    free(t->lowerBounds);
  }
  if(t->upperBounds)
  {
    free(t->upperBounds);
  }
  if(t->transformer)
  {
    freeTypeTransform(t->transformer);
    t->transformer=(void*)0xbaadf00d;
  }
  free(t);
}

TypeInfo* duplicateTypeInfo(const TypeInfo* t)
{
  TypeInfo* result=zmalloc(sizeof(TypeInfo));
  memcpy(result,t,sizeof(TypeInfo));
  result->rc.refcount=0;
  result->name=strdup(t->name);
  result->fields=zmalloc(sizeof(char*)*t->numFields);
  result->fieldOffsets=zmalloc(sizeof(int)*t->numFields);
  result->fieldTypes=zmalloc(sizeof(TypeInfo*)*t->numFields);
  for(int i=0;i<t->numFields;i++)
  {
    result->fields[i]=strdup(t->fields[i]);
    result->fieldOffsets[i]=t->fieldOffsets[i];
    result->fieldTypes[i]=t->fieldTypes[i];
    grabRefCounted((RC*)result->fieldTypes[i]);
  }
  result->pointedType=t->pointedType;
  result->depth=t->depth;
  result->lowerBounds=zmalloc(t->depth*sizeof(int));
  memcpy(result->lowerBounds,t->lowerBounds,t->depth*sizeof(int));
  result->upperBounds=zmalloc(t->depth*sizeof(int));
  memcpy(result->upperBounds,t->upperBounds,t->depth*sizeof(int));
  return result;
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

