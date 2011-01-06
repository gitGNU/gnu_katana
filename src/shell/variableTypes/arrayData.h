/*
  File: shell/variableData/arrayData.h
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
  Description: Data type for storing other variables in an array
*/

#ifndef array_variable_data_h
#define array_variable_data_h

#include "shell/variable.h"
#include <vector>

//todo: this is not done in a sensible manner, I should just create
//a generalized array type instead

class ShellArrayVariableData : public ShellVariableData
{
 public:
  //data will not be duplicated, it should be a copy that
  //rawVariableData is free to free() upon its own destruction
  ShellArrayVariableData();
  ~ShellArrayVariableData();
  void appendItem(ShellVariableData* item);
  //the returned pointer is valid until the next call to getData
  virtual ParamDataResult* getData(ShellParamCapability dataType,int idx);
  virtual bool isCapable(ShellParamCapability cap,int idx=0);
  virtual int getEntityCount();  
 protected:
  std::vector<ShellVariableData*> items;
  
};

#endif
// Local Variables:
// mode: c++
// End:
