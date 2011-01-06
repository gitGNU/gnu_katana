/*
  File: shell/variableData/elfSectionData.cpp
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

#include "elfSectionData.h"


//data will not be duplicated, it should be a copy that
//rawVariableData is free to free() upon its own destruction
ShellElfSectionVariableData::ShellElfSectionVariableData(byte* data,int len,SectionHeaderData* hdr)
{
  SectionEntry entry;
  entry.data=(byte*)zmalloc(len);
  memcpy(entry.data,data,len);
  entry.len=len;
  entry.header=NULL;
  if(hdr)
  {
    entry.header=(SectionHeaderData*)zmalloc(sizeof(SectionHeaderData));
    memcpy(entry.header,hdr,sizeof(SectionHeaderData));
  }
  sections.push_back(entry);
}

ShellElfSectionVariableData::~ShellElfSectionVariableData()
{
  for(uint i=0;i<sections.size();i++)
  {
    free(sections[i].data);
    free(sections[i].hexString);
    free(sections[i].header);
  }
}

//the returned pointer is valid until the next call to getData
ParamDataResult* ShellElfSectionVariableData::getData(ShellParamCapability dataType,int idx)
{
  initResult();
  switch(dataType)
  {
  case SPC_RAW_DATA:
    this->result->type=SPC_RAW_DATA;
    this->result->u.rawData.len=this->sections[idx].len;
    this->result->u.rawData.data=this->sections[idx].data;
    break;
  case SPC_SECTION_HEADER:
    {
      this->result->type=SPC_SECTION_HEADER;
      this->result->u.shdr=this->sections[idx].header;
    }
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"ShellElfSectionVariableData asked for data type it cannot supply\n");
    return NULL;
  }
  return result;
}
  
bool ShellElfSectionVariableData::isCapable(ShellParamCapability cap,int idx)
{
  assert((uint)idx<=sections.size());
  switch(cap)
  {
  case SPC_RAW_DATA:
    return true;
    break;
  case SPC_SECTION_HEADER:
    return sections[idx].header!=NULL;
    break;
  default:
    return false;
  }
}
int ShellElfSectionVariableData::getEntityCount()
{
  return sections.size();
}
