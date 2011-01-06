/*
  File: shell/variableData/elfSectionData.h
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
  Description: Data type for storing one or more ELF sections and optionally their headers
*/

#ifndef elf_section_variable_data_h
#define elf_section_variable_data_h

#include "shell/variable.h"
#include <vector>

//todo: this is not done in a sensible manner, I should just create
//a generalized array type instead

class ShellElfSectionVariableData : public ShellVariableData
{
 public:
  //data will not be duplicated, it should be a copy that
  //rawVariableData is free to free() upon its own destruction
  ShellElfSectionVariableData(byte* data,int len,SectionHeaderData* hdr=NULL);
  ~ShellElfSectionVariableData();
  //the returned pointer is valid until the next call to getData
  virtual ParamDataResult* getData(ShellParamCapability dataType,int idx=0);
  virtual bool isCapable(ShellParamCapability cap,int idx=0);
  virtual int getEntityCount();  
 protected:
  struct SectionEntry
  {
    byte* data;
    int len;
    char* hexString;
    SectionHeaderData* header;
  };
  std::vector<SectionEntry> sections;
  
};

#endif
// Local Variables:
// mode: c++
// End:
