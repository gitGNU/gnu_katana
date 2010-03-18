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

//this function may no longer be needed, but it works just fine
//sets a's transformer field to a transformer
//which just copies everything over
void genStraightCopyTransform(TypeInfo* a,TypeInfo* b)
{
  assert(!a->transformer);
  a->transformer=zmalloc(sizeof(TypeTransform));
  a->transformer->from=a;
  a->transformer->to=b;
  a->transformer->straightCopy=true;
}

//return false if the two types are not
//identical in all regards
//if the types are not identical, store in type a
//the necessary transformation info to convert it to type b,
//if possible
//todo: cache results for determining that a transformation isn't needed
bool compareTypesAndGenTransforms(TypeInfo* a,TypeInfo* b)
{
  TypeTransform* transform=NULL;
  if(a->type!=b->type)
  {
    //don't know how to perform the transformation
    return false;
  }
  if(a->diffAgainst)
  {
    assert(ETS_NOT_DONE!=a->typediffStatus);
    if(a->diffAgainst!=b)
    {
      death("cannot transform a type to two different types\n");
    }
    if(ETS_DIFFERED==a->typediffStatus)
    {
      return false;
    }
    else
    {
      return true;
    }
  }

  a->diffAgainst=b;
  
  bool retval=true;
  if(strcmp(a->name,b->name) ||
     a->numFields!=b->numFields ||
     a->length != b->length)
  {
    logprintf(ELL_INFO_V2,ELS_TYPEDIFF,"Type %s changed because name, numFields, or length changed\n",a->name);
    retval=false;
  }

  switch(a->type)
  {
  case TT_UNION:
  case TT_STRUCT:
    //first for loop was just to determine if we needed to build the
    //offsets array
    for(int i=0;retval && i<a->numFields;i++)
    {
      //todo: do we need to update if just the name changes?
      //certainly won't need to relocate. Or do we just assume
      //that if the name changes it's all different
      if(a->fieldLengths[i]!=b->fieldLengths[i] ||
         strcmp(a->fieldTypes[i]->name,b->fieldTypes[i]->name) || strcmp(a->fields[i],b->fields[i]))
      {
        if(!strncmp(a->fieldTypes[i]->name,"anon_",5) && !strncmp(a->fieldTypes[i]->name,"anon_",5) && a->fieldTypes[i]->type==b->fieldTypes[i]->type && !strcmp(a->fields[i],b->fields[i]) && a->fieldLengths[i]==b->fieldLengths[i])
        {
          logprintf(ELL_WARN,ELS_TYPEDIFF,"Struct or union %s has member %s of type %s/%s. Because this is an anonymous type, we aren't automatically assuming that a small name change means a change of type, but this may not be what you want\n",a->name,a->fields[i],a->fieldTypes[i]->name,b->fieldTypes[i]->name);
        }
        else
        {
          retval=false;
          logprintf(ELL_INFO_V2,ELS_TYPEDIFF,"Struct or union %s changed because old/new members %s/%s changed\n",a->name,a->fieldTypes[i]->name,b->fieldTypes[i]->name);
          break;
        }
      }
    
    //todo: what if a field stays the same type, but that type changes
    //need to be able to support that
    }
    break;
  case TT_ARRAY:
    if(a->lowerBound != b->lowerBound || a->upperBound!=b->upperBound)
    {
      retval=false;
      logprintf(ELL_WARN,ELS_TYPEDIFF,"Generating type transformation for array %s/%s by assuming anything new has been put on the end of the array. This may not be what you want\n",a->name,b->name);
      if(a->lowerBound != b->lowerBound)
      {
        death("haven't actually figured out how to properly write a type transformer for arrays changing lower bound yet. Poke James to do this\n");
      }
      break;
    }
    //deliberately no break here because want to check pointed type too
  case TT_POINTER:
  case TT_CONST:
    //this will generate the necessary transformation
    if(!compareTypesAndGenTransforms(a->pointedType,b->pointedType))
    {
      retval=false;
      logprintf(ELL_INFO_V2,ELS_TYPEDIFF,"Pointer or const type %s changed because type it refers to (%s) changed\n",a->name,a->pointedType->name);
      break;
    }
    break;
  case TT_BASE:
  case TT_VOID:
  case TT_ENUM://if there was a change it would have been caught by the length check above
    break;
  case TT_SUBROUTINE_TYPE:
    //subroutine type isn't a real type,
    //it exists only to make function pointers possible,
    //it can't change
    retval=true;
  }
    

  if(retval)
  {
    a->typediffStatus=ETS_SAME;
    return true;
  }

  a->typediffStatus=ETS_DIFFERED;
  
  transform=zmalloc(sizeof(TypeTransform));
  a->transformer=transform;//do it up here in case we recurse on this type
  transform->from=a;
  transform->to=b;

  if(TT_UNION==a->type || TT_ENUM==a->type || TT_ARRAY==a->type)
  {
    //a straight copy is the only way we can do a union, because we don't know
    //what's inside it. If we detect that a straight copy won't work,
    //we'll bail later

    //it's not really the way we want to do an array, but arrays are pretty opaque
    transform->straightCopy=true;
  }

    
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
    
    switch(fieldTypeOld->type)
    {
    case TT_BASE:
    case TT_ENUM:
      transform->fieldTransformTypes[i]=EFTT_COPY;
      break;
    case TT_STRUCT:
      transform->fieldTransformTypes[i]=EFTT_RECURSE;
      break;
    case TT_UNION:
      transform->fieldTransformTypes[i]=EFTT_RECURSE;
      break;
    case TT_POINTER:
    case TT_CONST:
      if(!compareTypesAndGenTransforms(fieldTypeOld->pointedType,fieldTypeNew->pointedType))
      {
        if(!fieldTypeOld->pointedType->transformer)
        {
          freeTypeTransform(transform);
          logprintf(ELL_WARN,ELS_TYPEDIFF,"Unable to generate transformation for field types");
          return false;
        }
      }
      if(fieldTypeOld->pointedType->transformer)
      {
        //points to something that needs dealing with
        transform->fieldTransformTypes[i]=EFTT_RECURSE;
      }
      else
      {
        //points to something that's fine, so we can just go ahead and copy the pointer,
        //no need to relocate everything
        transform->fieldTransformTypes[i]=EFTT_COPY;
      } 
      break;
    default:
      death("unsupported type %i in generating transform in compareTypes. Poke james to write in support\n",fieldTypeOld->type);
    }
    if(fieldTypeOld->transformer && fieldTypeOld->transformer->to != fieldTypeNew)
    {
      death("Cannot transform a type to two different types\n");
    }
      
    if(EFTT_RECURSE==transform->fieldTransformTypes[i])
    {
      if(!compareTypesAndGenTransforms(fieldTypeOld,fieldTypeNew))
      {
        if(!fieldTypeOld->transformer)
        {
          freeTypeTransform(transform);
          logprintf(ELL_WARN,ELS_TYPEDIFF,"Unable to generate transformation for field types");
          return false;
        }
        if(TT_UNION==a->type)
        {
          death("Cannot generate transformation for union type %s because its member type %s has changed and there is no way to know how to apply fixups when the type is unknown\n",a->name,fieldTypeNew->name);
        }
      }
    }
    transform->fieldOffsets[i]=getOffsetForField(b,a->fields[i]);
    //todo: how exactly to handle base type changing if different size
    //since we're on a little-endian system now, things
    //will just get zero-padded, which *should* be ok
    
    //todo: in general need to be able to support fields of struct type,
    //they're not really supported right now
  }
  return false;
}
