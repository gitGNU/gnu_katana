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

ShellVariable::ShellVariable(char* name)
{
  this->name=strdup(name);
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

char* ShellVariable::getString()
{
  return this->data->getString();
}
ElfInfo* ShellVariable::getElfObject()
{
  return this->data->getElfObject();
}
void* ShellVariable::getRawData(int* byteLenOut)
{
  return this->data->getRawData(byteLenOut);
}
GElf_Shdr ShellVariable::getSectionHeader()
{
  return this->data->getSectionHeader();
}
bool ShellVariable::isCapable(ShellParamCapability cap)
{
  return this->data->isCapable(cap);
}

//Now the stuff for the data types.
//by default all of the get methods are unable to complete

char* ShellVariableData::getString()
{
  logprintf(ELL_WARN,ELS_SHELL,"This type of variable cannot yield a string\n");
  return NULL;
}

ElfInfo* ShellVariableData::getElfObject()
{
  logprintf(ELL_WARN,ELS_SHELL,"This type of variable cannot yield an ELF object\n");
  return NULL;
}

void* ShellVariableData::getRawData(int* byteLenOut)
{
  logprintf(ELL_WARN,ELS_SHELL,"This type of variable cannot yield raw data\n");
  *byteLenOut=0;
  return NULL;
}

GElf_Shdr ShellVariableData::getSectionHeader()
{
  logprintf(ELL_WARN,ELS_SHELL,"This type of variable cannot yield a section header\n");
  GElf_Shdr result;
  memset(&result,0,sizeof(result));
  return result;
}
bool ShellVariableData::isCapable(ShellParamCapability cap)
{
  return false;
}
