/*
  File: shell/param.cpp
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version. Regardless of
  which version is chose, the following stipulation also applies:
    
  Any redistribution must include copyright notice attribution to
  Dartmouth College as well as the Warranty Disclaimer below, as well as
  this list of conditions in any related documentation and, if feasible,
  on the redistributed software; Any redistribution must include the
  acknowledgment, “This product includes software developed by Dartmouth
  College,” in any related documentation and, if feasible, in the
  redistributed software; and The names “Dartmouth” and “Dartmouth
  College” may not be used to endorse or promote products derived from
  this software.  

  WARRANTY DISCLAIMER

  PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
  SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
  OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
  HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
  DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
  EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
  SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
  PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
  OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

  IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
  WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
  PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
  ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
  INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
  INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
  DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
  THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
  PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGES.
    
  The complete text of the license may be found in the file COPYING
  which should have been distributed with this software. The GNU
  General Public License may be obtained at
  http://www.gnu.org/licenses/gpl.html
    
  Project:  Katana
  Date: December 2010
  Description: Class representing parameters used as arguments to commands in the Katana shell
*/

#include "param.h"

//The default shell parameter is just a string
//ShellVariables are also a type of ShellParameter
ShellParam::ShellParam()
  :result(NULL),stringValue(NULL)
{
}

ShellParam::ShellParam(char* string)
  :result(NULL),stringValue(string)
{
}

ShellParam::~ShellParam()
{
  free(this->stringValue);
  free(result);
}

//the returned pointer is valid until the next call to getData
ParamDataResult* ShellParam::getData(ShellParamCapability dataType,int idx)
{
  switch(dataType)
  {
  case SPC_STRING_VALUE:
    if(!this->result)
    {
      this->result=(ParamDataResult*)zmalloc(sizeof(ParamDataResult));
      MALLOC_CHECK(this->result);
    }
    this->result->type=dataType;
    this->result->u.str=this->stringValue;
    return result;
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"Parameter asked for data type it cannot supply\n");
    return NULL;
  }
}

bool ShellParam::isCapable(ShellParamCapability cap,int idx)
{
  if(cap==SPC_STRING_VALUE)
  {
    return true;
  }
  return false;
}

char* ShellParam::getString(int idx)
{
  if(!isCapable(SPC_STRING_VALUE))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempting to get the string value for a variable that cannot be converted to a string\n");
    return NULL;
  }
  ParamDataResult* result=this->getData(SPC_STRING_VALUE,idx);
  assert(result && result->type==SPC_STRING_VALUE);
  return result->u.str;
}

int ShellParam::getInt(int idx)
{
  if(!isCapable(SPC_INT_VALUE))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempting to get int value for a variable that cannot be converted to an int\n");
    return 0;
  }
  ParamDataResult* result=this->getData(SPC_INT_VALUE,idx);
  assert(result && result->type==SPC_INT_VALUE);
  return result->u.intval;
}

ElfInfo* ShellParam::getElfObject(int idx)
{
  if(!isCapable(SPC_ELF_VALUE))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempting to get the ELF value for a variable that cannot be converted to an ELF object\n");
    return NULL;
  }
  ParamDataResult* result=this->getData(SPC_ELF_VALUE,idx);
  assert(result && result->type==SPC_ELF_VALUE);
  return result->u.elf;
}

byte* ShellParam::getRawData(int* numBytesOut,int idx)
{
  if(!isCapable(SPC_RAW_DATA))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempting to get the raw data value for a variable that cannot be converted to raw data\n");
    return NULL;
  }
  ParamDataResult* result=this->getData(SPC_RAW_DATA,idx);
  *numBytesOut=result->u.rawData.len;
  return result->u.rawData.data;
}

SectionHeaderData* ShellParam::getSectionHeader(int idx)
{
  if(!isCapable(SPC_SECTION_HEADER))
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempting to get the section header value for a variable that cannot be converted to a section header\n");
    return NULL;
  }
  ParamDataResult* result=this->getData(SPC_SECTION_HEADER,idx);
  return result->u.shdr;
}

void ShellParam::setValue(char* string)
{
  this->stringValue=strdup(string);
}

ShellIntParam::ShellIntParam(sword_t intval)
  :value(intval)
{
}
ShellIntParam::~ShellIntParam()
{
}

ParamDataResult* ShellIntParam::getData(ShellParamCapability dataType,int idx)
{
  switch(dataType)
  {
  case SPC_INT_VALUE:
    if(!this->result)
    {
      this->result=(ParamDataResult*)zmalloc(sizeof(ParamDataResult));
      MALLOC_CHECK(this->result);
    }
    this->result->type=dataType;
    this->result->u.intval=this->value;
    return result;
    break;
  default:
    logprintf(ELL_WARN,ELS_SHELL,"Parameter asked for data type it cannot supply\n");
    return NULL;
  }
}
bool ShellIntParam::isCapable(ShellParamCapability cap,int idx)
{
  if(cap==SPC_INT_VALUE)
  {
    return true;
  }
  return false;
}

