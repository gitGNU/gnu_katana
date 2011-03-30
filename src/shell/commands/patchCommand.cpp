/*
  File: patchCommand.cpp
  Author: James Oakley
  Copyright (C): 2011 Dartmouth College
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
  Date: March 2011
  Description: Command class for the katana shell. Performs actions
  related to hotpatching.

*/

#include "patchCommand.h"
extern "C"
{
#include "patchwrite/patchwrite.h"
#include "util/path.h"
}

  //constructor for patch generation
PatchCommand::PatchCommand(PatchOperation op,ShellParam* oldObjectsDir,ShellParam* newObjectsDir,ShellParam* executableName)
  :op(op),oldObjectsDirP(oldObjectsDir),newObjectsDirP(newObjectsDir),executableNameP(executableName),patchfileP(NULL),pidP(NULL)
{
  assert(op==PO_GENERATE_PATCH);
  newObjectsDirP->grab();
  oldObjectsDirP->grab();
  executableNameP->grab();
}
  //constructor for patch application
PatchCommand::PatchCommand(PatchOperation op,ShellParam* patchfile,ShellParam* pid)
  :op(op),oldObjectsDirP(NULL),newObjectsDirP(NULL),executableNameP(NULL),patchfileP(patchfile),pidP(pid)
{
  assert(op==PO_APPLY_PATCH);
  patchfileP->grab();
  pidP->grab();
}


PatchCommand::~PatchCommand()
{
  if(newObjectsDirP)
  {
    newObjectsDirP->drop();
  }
  if(oldObjectsDirP)
  {
    oldObjectsDirP->drop();
  }
  if(executableNameP)
  {
    executableNameP->drop();
  }
  if(patchfileP)
  {
    patchfileP->drop();
  }
  if(pidP)
  {
    pidP->drop();
  }
}

void PatchCommand::generatePatch()
{
  if(!this->outputVariable)
  {
    logprintf(ELL_WARN,ELS_SHELL,"patch gen command given with no target, cowardly refusing to do anything\n");
    return;
  }
  char* oldSrcTree=oldObjectsDirP->getString();
  char* newSrcTree=newObjectsDirP->getString();
  char* execName=executableNameP->getString();
  if(!oldSrcTree || !newSrcTree || !execName)
  {
    throw "old or new object/executable path not specified";
  }
  char* oldBinPath=joinPaths(oldSrcTree,execName);
  char* newBinPath=joinPaths(newSrcTree,execName);

  FILE* tmpOutfile=tmpfile();
  ElfInfo* patch=createPatch(oldSrcTree,newSrcTree,oldBinPath,newBinPath,tmpOutfile);
  if(!patch)
  {
    throw "Unable to generate patch";
  }
  this->outputVariable->setValue(patch);
}

void PatchCommand::applyPatch()
{
  throw "Not yet implemented\n";
}

void PatchCommand::execute()
{
  switch(op)
  {
  case PO_GENERATE_PATCH:
    generatePatch();
    break;
  case PO_APPLY_PATCH:
    applyPatch();
    break;
  default:
    death("unknown operation in PatchCommand\n");
  }
}
