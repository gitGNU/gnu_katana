/*
  File: shell/variableData/arrayData.cpp
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
  Description: Data type for storing other items in an array
*/

#include "arrayData.h"
ShellArrayVariableData::ShellArrayVariableData()
{
}
ShellArrayVariableData::~ShellArrayVariableData()
{
  for(uint i=0;i<items.size();i++)
  {
    //todo: do we need to reference count these?
    delete items[i];
  }
}
void ShellArrayVariableData::appendItem(ShellVariableData* item)
{
  //todo: do we need to reference count these?
  items.push_back(item);
}

bool ShellArrayVariableData::isCapable(ShellParamCapability cap,int idx)
{
  assert(idx<(int)items.size());
  switch(cap)
  {
  case SPC_VARIABLE_DATA:
    return true;
  default:
    return false;
  }
}
int ShellArrayVariableData::getEntityCount()
{
  return items.size();
}

//the returned pointer is valid until the next call to getData
ParamDataResult* ShellArrayVariableData::getData(ShellParamCapability dataType,int idx)
{
  initResult();
  switch(dataType)
  {
  case SPC_VARIABLE_DATA:
    this->result->type=SPC_VARIABLE_DATA;
    this->result->u.variableData=this->items[idx];
    return result;
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"ArrayData asked for data type it cannot supply\n");
    return NULL;
  }
}
