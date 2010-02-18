/*
  File: typediff.c
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: functions relating to finding difference between types and building
               transformers for them
*/

#include "typediff.h"
#include <assert.h>
#include "types.h"
#include "util/logging.h"

int getOffsetForField(TypeInfo* type,char* name)
{
  int offset=0;
  for(int i=0;i<type->numFields;i++)
  {
    if(!strcmp(name,type->fields[i]))
    {
      logprintf(ELL_INFO_V4,ELS_MISC,"offset for field returning %i for field %s\n",offset,name);
      return offset;
    }
    offset+=type->fieldLengths[i];
  }
  return FIELD_DELETED;
}

int getIndexForField(TypeInfo* type,char* name)
{
  for(int i=0;i<type->numFields;i++)
  {
    if(!strcmp(name,type->fields[i]))
    {
      return i;
    }
  }
  return FIELD_DELETED;
}

//return false if the two types are not
//identical in all regards
//if the types are not identical, store in type a
//the necessary transformation info to convert it to type b,
//if possible
bool compareTypesAndGenTransforms(TypeInfo* a,TypeInfo* b)
{
  TypeTransform* transform=NULL;
  if(a->type!=b->type)
  {
    //don't know how to perform the transformation
    return false;
  }
  bool retval=true;
  if(strcmp(a->name,b->name) ||
     a->numFields!=b->numFields)
  {
    retval=false;
    if(!transform)
    {
      transform=zmalloc(sizeof(TypeTransform));
    }
  }

  //first for loop was just to determine if we needed to build the
  //offsets array
  for(int i=0;retval && i<a->numFields;i++)
  {
    //todo: do we need to update if just the name changes?
    //certainly won't need to relocate
    if(a->fieldLengths[i]!=b->fieldLengths[i] ||
       strcmp(a->fieldTypes[i]->name,b->fieldTypes[i]->name))
    {
      retval=false;
      if(!transform)
      {
        transform=zmalloc(sizeof(TypeTransform));
      }
    }
    
    //todo: what if a field stays the same type, but that type changes
    //need to be able to support that
  }

  if(transform)
  {
    //now build the offsets array
    transform->fieldOffsets=zmalloc(sizeof(int)*a->numFields);
    transform->fieldTransformTypes=zmalloc(sizeof(int)*a->numFields);
    for(int i=0;i<a->numFields;i++)
    {
      int idxInB=getIndexForField(b,a->fields[i]);
      TypeInfo* fieldTypeOld=a->fieldTypes[i];
      TypeInfo* fieldTypeNew=b->fieldTypes[idxInB];
      if(fieldTypeOld->type != fieldTypeNew->type)
      {
        //type changed too drastically even if the name hasn't,
        //so we can't really patch that
        //todo: issue a warning?
        transform->fieldOffsets[i]=FIELD_DELETED;
        transform->fieldTransformTypes[i]=EFTT_DELETE;
        continue;
      }
      int offset;
      switch(fieldTypeOld->type)
      {
      case TT_BASE:
        offset=getOffsetForField(b,a->fields[i]);
        transform->fieldTransformTypes[i]=EFTT_COPY;
        break;
      case TT_STRUCT:
        offset=getOffsetForField(b,a->fields[i]);
        transform->fieldTransformTypes[i]=EFTT_RECURSE;
        break;
      default:
        death("unsupported type in generating transform in compareTypes. Poke james to write in support\n");
      }
      if(fieldTypeOld->transformer && fieldTypeOld->transformer->to != fieldTypeNew)
      {
        death("Cannot transform a type to two different types\n");
      }
      if(EFTT_RECURSE==transform->fieldTransformTypes[i] && !fieldTypeOld->transformer)
      {
        if(!compareTypesAndGenTransforms(fieldTypeOld,fieldTypeNew))
        {
          if(!fieldTypeOld->transformer)
          {
            freeTypeTransform(transform);
            logprintf(ELL_WARN,ELS_TYPEDIFF,"Unable to generate transformation for field types");
            return false;
          }
        }
      }
      transform->fieldOffsets[i]=offset;
      //todo: how exactly to handle base type changing if different size
      //since we're on a little-endian system now, things
      //will just get zero-padded, which *should* be ok
    
      //todo: in general need to be able to support fields of struct type,
      //they're not really supported right now
    }
    transform->from=a;
    transform->to=b;
  }
  

  //todo: support pointer types
  a->transformer=transform;
  return retval;
}
