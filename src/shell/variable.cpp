/*
  File: shell/variable.cpp
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
  Description: Class representing variables used in the Katana shell
*/


#include "variable.h"
#include "variableTypes/elfVariableData.h"
#include "variableTypes/rawVariableData.h"
#include "variableTypes/elfSectionData.h"
#include "variableTypes/arrayData.h"

ShellVariable::ShellVariable(char* name)
{
  this->name=strdup(name);
  this->data=NULL;
}

ShellVariable::~ShellVariable()
{
  free(name);
  delete data;
}

void ShellVariable::setValue(ElfInfo* e)
{
  if(this->data)
  {
    delete this->data;
  }
  this->data=new ShellElfVariableData(e);
}

void ShellVariable::setValue(byte* data,int dataLen)
{
  if(this->data)
  {
    delete this->data;
  }
  this->data=new ShellRawVariableData(data,dataLen);
}

void ShellVariable::setValue(byte* data,int dataLen,SectionHeaderData* header)
{
  if(this->data)
  {
    delete this->data;
  }
  this->data=new ShellElfSectionVariableData(data,dataLen,header);
}

void ShellVariable::makeArray(ShellVariableData** items,int cnt)
{
  if(this->data)
  {
    delete this->data;
  }
  ShellArrayVariableData* array=new ShellArrayVariableData();
  for(int i=0;i<cnt;i++)
  {
    array->appendItem(items[i]);
  }
  this->data=array;
}

//the returned pointer is valid until the next call to getData
ParamDataResult* ShellVariable::getData(ShellParamCapability dataType,int idx)
{
  return this->data->getData(dataType,idx);
}

//some types of variables can hold multiple pieces of the same sort
//of data, for example dwarfscript compile can emit a variable which
//may contain data for multiple sections
int ShellVariable::getEntityCount()
{
  return this->data->getEntityCount();
}

bool ShellVariable::isCapable(ShellParamCapability cap,int idx)
{
  if(!this->data)
  {
    logprintf(ELL_WARN,ELS_SHELL,"Attempt to use uninitialized variable %s\n",this->name);
    throw "Unitialized variable access";

  }
  return this->data->isCapable(cap,idx);
}


//Now the stuff for the data types.
//by default all of the get methods are unable to complete

ShellVariableData::ShellVariableData()
  :result(NULL)
{

}

//the returned pointer is valid until the next call to getData
ParamDataResult* ShellVariableData::getData(ShellParamCapability dataType,int idx)
{
  logprintf(ELL_WARN,ELS_SHELL,"This type of variable cannot yield this type of data\n");
  return NULL;
}

bool ShellVariableData::isCapable(ShellParamCapability cap,int idx)
{
  return false;
}

//some types of variables can hold multiple pieces of the same sort
//of data, for example dwarfscript compile can emit a variable which
//may contain data for multiple sections
int ShellVariableData::getEntityCount()
{
  return 1;
}

void ShellVariableData::initResult()
{
  if(!this->result)
  {
    this->result=(ParamDataResult*)zmalloc(sizeof(ParamDataResult));
    MALLOC_CHECK(this->result);
  }

}
