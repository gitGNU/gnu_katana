/*
  File: commandLine.c
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
  Date: November 2010
  Description: Parsing of the command line
*/

#include "config.h"
#include <unistd.h>
#include "util/util.h"
#include "util/logging.h"

void configureFromCommandLine(int argc,char** argv)
{
  int opt;
  while((opt=getopt(argc,argv,"hcsldHgpo:"))>0)
  {
    switch(opt)
    {
    case 'g':
      if(config.mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      config.mode=EKM_GEN_PATCH;
      break;
    case 'p':
      if(config.mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      config.mode=EKM_APPLY_PATCH;
      break;
    case 'o':
      config.outfileName=strdup(optarg);
      break;
    case 'l':
      if(config.mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      config.mode=EKM_INFO;
      break;
    case 's':
      if(EKM_NONE==config.mode)
      {
        death("Must specify a mode (-g,-p, or -l) before any other options");
      }
      if(EKM_INFO==config.mode)
      {
        setFlag(EKCF_P_STOP_TARGET,true);
      }
      else
      {
        death("-s flag has no meaning for this mode\n");
      }
      break;
    case 'c':
      loadConfigurationFile(optarg);
      break;
    case 'H':
      setFlag(EKCF_EH_FRAME,true);
      break;
    case 'd':
      //this is a debug option at the moment. It is not intended to be
      //used generally. It's functionality may change from time to
      //time. At the moment it is a new major mode of operation which
      //expects there to be one argument after the options which
      //specifies a program to read. That program is loaded and then
      //written out again.
      config.mode=EKM_TEST_PASSTHROUGH;
      break;
    case 'h':
      //todo: write help information
      fprintf(stdout,"Help has not yet been written. Consult the katana manpage or documentation\n");
      exit(0);
    }
  }
  if(EKM_NONE==config.mode)
  {
    death("One of -g (gen patch) or -p (apply patch) or -l (list info about patch) must be specified\n");
  }

  if(EKM_GEN_PATCH==config.mode)
  {
    if(argc-optind<3)
    {
      death("Usage to generate patch: katana -g [-o OUT_FILE] OLD_SOURCE_TREE NEW_SOURCE_TREE EXEC");
    }
    config.oldSourceTree=argv[optind];
    config.newSourceTree=argv[optind+1];
    config.objectName=argv[optind+2];
    if(!strcmp(config.oldSourceTree,config.newSourceTree))
    {
      death("OLD_SOURCE_TREE and NEW_SOURCE_TREE must be different paths\n");
    }
  }
  else if(EKM_APPLY_PATCH==config.mode)
  {
    if(argc-optind<2)
    {
      death("Usage to apply patch: katana -p [OPTIOJNS] PATCH_FILE PID\nSee the man page for a description of options\n");
    }
    config.objectName=argv[optind];
    printf("patch file is %s\n",config.objectName);
    config.pid=atoi(argv[optind+1]);
    fprintf(stderr,"pid is %i\n",config.pid);
  }
  else if(EKM_INFO==config.mode)
  {
    if(argc-optind<1)
    {
      death("Usage to list patch info: katana -l PATCH_FILE\n");
    }
    config.objectName=argv[optind];
    logprintf(ELL_INFO_V3,ELS_MISC,"patch file is %s\n",config.objectName);

    
  }
}
