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
#include "patcher/target.h"
#include "elfparse.h"
#include "patcher/hotpatch.h"
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

void sigsegReceived(int signum)
{
  death("katana segfaulting. . .\n");
}


int main(int argc,char** argv)
{
  #ifdef DEBUG
  system("rm -rf /tmp/katana-$USER/patched/*");
  #endif
  if(argc<3)
  {
    death("Usage: katana -g [-o OUT_FILE] OLD_BINARY NEW_BINARY \n\tOr: katana -p PATCH_FILE PID");
  }

  loggingDefaults();
  
  struct sigaction act;
  memset(&act,0,sizeof(struct sigaction));
  act.sa_handler=&sigsegReceived;
  sigaction(SIGSEGV,&act,NULL);
  int opt;
  bool genPatch=false;
  bool applyPatch=false;
  char* outfile=NULL;
  while((opt=getopt(argc,argv,"gpo:"))>0)
  {
    switch(opt)
    {
    case 'g':
      genPatch=true;
      break;
    case 'p':
      applyPatch=true;
      break;
    case 'o':
      outfile=optarg;
      break;
    }
  }
  if(applyPatch && genPatch)
  {
    death("cannot both generate and apply a patch in a single run\n");
  }
  if(!(applyPatch | genPatch))
  {
    death("One of -g (gen patch) or -p (apply patch) must be specified\n");
  }
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    death("Failed to init ELF library\n");
  }
  if(genPatch)
  {
    if(argc-optind<2)
    {
      death("Usage to generate patch: katana -g [-o OUT_FILE] OLD_BINARY NEW_BINARY");
    }
    char* oldBinary=argv[optind];
    char* newBinary=argv[optind+1];
    if(!outfile)
    {
      outfile=zmalloc(strlen(oldBinary)+5);
      strcpy(outfile,oldBinary);
      strcat(outfile,".po");
    }
    ElfInfo* newBinaryElfInfo=openELFFile(newBinary);
    findELFSections(newBinaryElfInfo);
    printf("reading dwarf types from new binary\n############################################\n");
    DwarfInfo* diPatched=readDWARFTypes(newBinaryElfInfo);
    oldBinElfInfo=openELFFile(oldBinary);
    findELFSections(oldBinElfInfo);
    printf("reading dwarf types from old binary\n#############################################\n");
    DwarfInfo* diPatchee=readDWARFTypes(oldBinElfInfo);
    printf("outfile is %s",outfile);
    writePatch(diPatchee,diPatched,outfile,oldBinElfInfo,newBinaryElfInfo);
    endELF(oldBinElfInfo);
    endELF(newBinaryElfInfo);
  }
  else //applyPatch
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
    startPtrace();
    readAndApplyPatch(pid,oldBinElfInfo,patch);
    endPtrace();
  }
  return 0;
}

