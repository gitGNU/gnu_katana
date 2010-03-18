/*
  File: typepatch.c
  Author: James Oakley
  Project: Katana
  Date: January, 2010
  Description: Preliminary patching program just for patching main_v0.c's structure Foo to have an extra field
               The first version of this program is going to be very simple. It will perform the following steps
               1. Load main_v0 with dwarfdump and determine where the variable bar is stored
               2. Find all references to the variable bar
               3. Attach to the running process with ptrace
               4. Make the process execute mmap to allocate space for a new data segment
               5. Copy over field1 and field2 from Foo bar into the new memory area and zero the added field
               6. Fixup all locations referring to the old address of bar with the new address and set the offset accordingly (should be able to get information for fixups from the rel.text section)
  Usage: katana OLD_BINARY NEW_BINARY PID
         PID is expected to be a pid of a running process build from OLD_BINARY
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
#include <libdwarf.h>
#include "dwarf.h"
#include "dwarftypes.h"
#include "types.h"
#include "elfparse.h"
#include <signal.h>
#include "patchwrite/patchwrite.h"
int pid;//pid of running process
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

void sigsegReceived(int signum)
{
  death("katana segfaulting. . .\n");
}

typedef enum
{
  EKM_NONE,
  EKM_GEN_PATCH,
  EKM_APPLY_PATCH,
  EKM_PATCH_INFO
} E_KATANA_MODE;

int main(int argc,char** argv)
{
  #ifdef DEBUG
  system("rm -rf /tmp/katana-$USER/patched/*");
  #endif
  loggingDefaults();
  setDefaultConfig();
  
  struct sigaction act;
  memset(&act,0,sizeof(struct sigaction));
  act.sa_handler=&sigsegReceived;
  sigaction(SIGSEGV,&act,NULL);
  int opt;
  E_KATANA_MODE mode=EKM_NONE;
  char* outfile=NULL;
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    death("Failed to init ELF library\n");
  }
  while((opt=getopt(argc,argv,"lgpo:"))>0)
  {
    switch(opt)
    {
    case 'g':
      if(mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      mode=EKM_GEN_PATCH;
      break;
    case 'p':
      if(mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      mode=EKM_APPLY_PATCH;
      break;
    case 'o':
      outfile=optarg;
      break;
    case 'l':
      if(mode!=EKM_NONE)
      {
        death("only one of g,p,l may be specified\n");
      }
      mode=EKM_PATCH_INFO;
      break;
      
    }
  }
  if(EKM_NONE==mode)
  {
    death("One of -g (gen patch) or -p (apply patch) or -l (list info about patch) must be specified\n");
  }
  if(EKM_GEN_PATCH==mode)
  {
    if(argc-optind<3)
    {
      death("Usage to generate patch: katana -g [-o OUT_FILE] OLD_SOURCE_TREE NEW_SOURCE_TREEEXEC");
    }
    char* oldSourceTree=argv[optind];
    char* newSourceTree=argv[optind+1];
    char* execRelName=argv[optind+2];
    char* oldBinPath=joinPaths(oldSourceTree,execRelName);
    char* newBinPath=joinPaths(newSourceTree,execRelName);
    
    if(!outfile)
    {
      outfile=zmalloc(strlen(oldBinPath)+5);
      strcpy(outfile,oldBinPath);
      strcat(outfile,".po");
    }
    printf("reading dwarf types from new binary\n############################################\n");

    printf("outfile is %s",outfile);
    writePatch(oldSourceTree,newSourceTree,oldBinPath,newBinPath,outfile);
    free(oldBinPath);
    free(newBinPath);
  }
  else if(EKM_APPLY_PATCH==mode)
  {
    if(argc-optind<2)
    {
      death("Usage to apply patch: katana -p PATCH_FILE PID");
    }
    char* patchFile=argv[optind];
    printf("patch file is %s\n",patchFile);
    pid=atoi(argv[optind+1]);
    oldBinElfInfo=getElfRepresentingProc(pid);
    findELFSections(oldBinElfInfo);
    ElfInfo* patch=openELFFile(patchFile);
    findELFSections(patch);
    patch->isPO=true;
    readAndApplyPatch(pid,oldBinElfInfo,patch);
  }
  else if(EKM_PATCH_INFO==mode)
  {
    if(argc-optind<1)
    {
      death("Usage to list patch info: katana -l PATCH_FILE");
    }
    char* patchFile=argv[optind];
    logprintf(ELL_INFO_V3,ELS_MISC,"patch file is %s\n",patchFile);
    ElfInfo* patch=openELFFile(patchFile);
    patch->isPO=true;//todo: this should be detected by openELFFile
    Map* fdeMap=readDebugFrame(patch);
    printf("*********Type and Function Info****************\n");
    printPatchDwarfInfo(patch,fdeMap);
    printf("\n*********Safety Info****************\n");
    printPatchUnsafeFuncsInfo(patch);
    printf("\n*********Type Transformation Rules****************\n");
    printPatchFDEInfo(patch);
  }
  else
  {
    death("unhandled katana mode");
  }
  return 0;
}

