/*
  File: shell/variableData/stringData.cpp
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
  Date: April 2011
  Description: Data type for storing one or more ELF sections and optionally their headers
*/

#include "stringData.h"

ShellStringVariableData::ShellStringVariableData(char* string)
{
  this->string=strdup(string);
}
ShellStringVariableData::~ShellStringVariableData()
{
  free(string);
}
  //the returned pointer is valid until the next call to getData
ParamDataResult* ShellStringVariableData::getData(ShellParamCapability dataType,int idx)
{
  initResult();
  switch(dataType)
  {
  case SPC_STRING_VALUE:
    this->result->type=SPC_STRING_VALUE;
    this->result->u.str=string;
    break;
  case SPC_RAW_DATA:
    this->result->type=SPC_RAW_DATA;
    this->result->u.rawData.data=(byte*)string;
    this->result->u.rawData.len=strlen(string);
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"ShellStringVariableData asked for data type it cannot supply\n");
    return NULL;
  }
  return result;
}
bool ShellStringVariableData::isCapable(ShellParamCapability cap,int idx)
{
  switch(cap)
  {
  case SPC_STRING_VALUE:
  case SPC_RAW_DATA:
    return true;
  default:
    return false;
  }
}

