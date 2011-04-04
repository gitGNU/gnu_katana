/*
  File: shell/variableData/stringData.h
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

#ifndef string_variable_data_h
#define string__variable_data_h

#include "shell/variable.h"


class ShellStringVariableData : public ShellVariableData
{
 public:

  ShellStringVariableData(char* string);
  ~ShellStringVariableData();
  //the returned pointer is valid until the next call to getData
  virtual ParamDataResult* getData(ShellParamCapability dataType,int idx=0);
  virtual bool isCapable(ShellParamCapability cap,int idx=0);
 protected:
  char* string;
  
};

#endif
// Local Variables:
// mode: c++
// End:
