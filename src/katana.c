/*
  File: katana.c
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

  Project: Katana
  Date: December, 2010
  Description:
*/
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include "dwarftypes.h"
#include "types.h"
#include "elfparse.h"
#include <signal.h>
#include "patchwrite/patchwrite.h"
ElfInfo* oldBinElfInfo=NULL;
#ifdef DEBUG
#include "katana-prelim.h"
#endif
#include <sys/stat.h>
#include "patcher/patchapply.h"
#include "patcher/versioning.h"
#include "util/logging.h"
#include "patchwrite/typediff.h"
#include "info/fdedump.h"
#include "info/dwinfo_dump.h"
#include "info/unsafe_funcs_dump.h"
#include "util/path.h"
#include "config.h"
#include "rewriter/rewrite.h"
#include "shell/shell.h"

void configureFromCommandLine(int argc,char** argv);

void sigsegReceived(int signum)
{
  death("katana segfaulting. . .\n");
}

int main(int argc,char** argv)
{
  #ifdef DEBUG
  system("rm -rf /tmp/katana-$USER/patched/*");
  #endif
  loggingDefaults();
  setDefaultConfig();

  struct stat s;
  if(0==stat("/etc/katana",&s))
  {
    loadConfigurationFile("~/.katana");
  }
  if(0==stat("~/.katana",&s))
  {
    loadConfigurationFile("~/.katana");
  }
  if(0==stat("~/.config/katana",&s))
  {
    loadConfigurationFile("~/.config/katana");
  }
  if(0==stat(".katana",&s))
  {
    loadConfigurationFile(".katana");
  }
  
  struct sigaction act;
  memset(&act,0,sizeof(struct sigaction));
  act.sa_handler=&sigsegReceived;
  sigaction(SIGSEGV,&act,NULL);
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    death("Failed to init ELF library\n");
  }
  configureFromCommandLine(argc,argv);
  if(EKM_SHELL==config.mode)
  {
    doShell(config.inputFile);
  }
  else if(EKM_GEN_PATCH==config.mode)
  {
    char* oldBinPath=joinPaths(config.oldSourceTree,config.objectName);
    char* newBinPath=joinPaths(config.newSourceTree,config.objectName);
    
    if(!config.outfileName)
    {
      config.outfileName=zmalloc(strlen(oldBinPath)+5);
      strcpy(config.outfileName,oldBinPath);
      strcat(config.outfileName,".po");
    }

    ElfInfo* patch=createPatch(config.oldSourceTree,config.newSourceTree,oldBinPath,newBinPath,NULL,config.outfileName);
    endELF(patch);
    free(oldBinPath);
    free(newBinPath);
  }
  else if(EKM_APPLY_PATCH==config.mode)
  {
    oldBinElfInfo=getElfRepresentingProc(config.pid);
    findELFSections(oldBinElfInfo);
    ElfInfo* patch=openELFFile(config.objectName);
    findELFSections(patch);
    patch->isPO=true;
    readAndApplyPatch(config.pid,oldBinElfInfo,patch);
    endELF(patch);
  }
  else if(EKM_INFO==config.mode)
  {
    ElfInfo* patch=openELFFile(config.objectName);
    Map* fdeMap=readDebugFrame(patch,isFlag(EKCF_EH_FRAME));
    printf("*********Type and Function Info****************\n");
    printPatchDwarfInfo(patch,fdeMap);

    //while katana is primarily for dealing with patches, it also has
    //some functionality for displaying general-purpose information
    //about DWARF and ELF information.
    if(patch->isPO)
    {
      printf("\n*********Safety Info****************\n");
      printPatchUnsafeFuncsInfo(patch);
    }
    if(patch->isPO)
    {
      printf("\n*********Type Transformation Rules****************\n");
    }
    else
    {
      printf("\n*********Call Frame Information****************\n");
    }
    printCallFrameInfo(stdout,&patch->callFrameInfo,patch);
  }
  else if(EKM_TEST_PASSTHROUGH==config.mode)
  {
    ElfInfo* object=openELFFile(config.objectName);
    rewrite(object,config.outfileName);
  }
  else
  {
    death("unhandled katana mode");
  }
  return 0;
}

