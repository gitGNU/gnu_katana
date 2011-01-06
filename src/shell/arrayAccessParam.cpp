/*
  File: shell/arrayAccessParam.cpp
  Author: James Oakley
  Copyright (C): 2011 James Oakley
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version.

  This file was not written while under employment by Dartmouth
  College and the attribution requirements on the rest of Katana do
  not apply to code taken from this file.
  Project:  katana
  Date: January 2011
  Description: Parameter in the shell that is actually access to an array
*/

#include "arrayAccessParam.h"
#include "variable.h"

ShellArrayAccessParam::ShellArrayAccessParam(ShellParam* array,int accessIdx)
  :array(array),accessIdx(accessIdx)
{
  this->array->grab();
}
ShellArrayAccessParam::~ShellArrayAccessParam()
{
  this->array->drop();
}
//the returned pointer is valid until the next call to getData
ParamDataResult* ShellArrayAccessParam::getData(ShellParamCapability dataType,int idx)
{
  if(!array->isCapable(SPC_VARIABLE_DATA,this->accessIdx))
  {
    throw "Invalid array access";
  }
  ParamDataResult* result=array->getData(SPC_VARIABLE_DATA,this->accessIdx);
  assert(result);
  ShellVariableData* variableReferenced=result->u.variableData;
  assert(variableReferenced);
  return variableReferenced->getData(dataType,idx);
}
bool ShellArrayAccessParam::isCapable(ShellParamCapability cap,int idx)
{
  if(!array->isCapable(SPC_VARIABLE_DATA,this->accessIdx))
  {
    throw "Invalid array access";
  }
  ParamDataResult* result=array->getData(SPC_VARIABLE_DATA,this->accessIdx);
  assert(result);
  ShellVariableData* variableReferenced=result->u.variableData;
  assert(variableReferenced);
  return variableReferenced->isCapable(cap,idx);
}




