/*
  File: shell/arrayAccessParam.h
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

#ifndef array_access_param_h
#define array_access_param_h

#include "param.h"

//The default shell parameter is just a string
//ShellVariables are also a type of ShellParameter
class ShellArrayAccessParam : public ShellParam
{
 public:
  ShellArrayAccessParam(ShellParam* array,int accessIdx);
  ~ShellArrayAccessParam();
  //the returned pointer is valid until the next call to getData
  virtual ParamDataResult* getData(ShellParamCapability dataType,int idx=0);
  virtual bool isCapable(ShellParamCapability cap,int idx=0);

 private:
  ShellParam* array;
  int accessIdx;
  
};

#endif
// Local Variables:
// mode: c++
// End:
