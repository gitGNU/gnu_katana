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

void sigsegReceived(int signum)
{
  death("katana segfaulting. . .\n");
}

void diffAndFixTypes(DwarfInfo* diPatchee,DwarfInfo* diPatched)

{
  //todo: handle addition of variables and also handle
  //      things moving between compilation units,
  //      perhaps group global objects from all compilation units
  //      together before dealing with them.
  List* cuLi1=diPatchee->compilationUnits;
  for(;cuLi1;cuLi1=cuLi1->next)
  {
    CompilationUnit* cu1=cuLi1->value;
    CompilationUnit* cu2=NULL;
    //find the corresponding compilation unit in the patched process
    //note: we assume that the number of compilation units is not so large
    //that using a hash table would give us much better performance than
    //just going through a list. If working on a truly enormous project it
    //might make sense to do this
    List* cuLi2=diPatched->compilationUnits;
    for(;cuLi2;cuLi2=cuLi2->next)
    {
      cu2=cuLi2->value;
      if(cu1->name && cu2->name && !strcmp(cu1->name,cu2->name))
      {
        break;
      }
      cu2=NULL;
    }
    if(!cu2)
    {
      fprintf(stderr,"WARNING: the patched version omits an entire compilation unit present in the original version.\n");
      if(cu1->name)
      {
        fprintf(stderr,"Missing cu is named %s\n",cu1->name);
      }
      else
      {
        fprintf(stderr,"The compilation unit in the patchee version does not have a name\n");
      }
      //todo: we also need to handle a compilation unit being present
      //in the patched version and not the patchee
      break;
    }
    cu1->presentInOtherVersion=true;
    cu2->presentInOtherVersion=true;
    
    TransformationInfo* trans=zmalloc(sizeof(TransformationInfo));
    trans->typeTransformers=dictCreate(100);//todo: get rid of magic # 100
    printf("Examining compilation unit %s\n",cu1->name);
    VarInfo** vars1=(VarInfo**)dictValues(cu1->tv->globalVars);
    //todo: handle addition of variables in the patch
    VarInfo* var=vars1[0];
    Dictionary* patchVars=cu2->tv->globalVars;
    for(int i=0;var;i++,var=vars1[i])
    {
      printf("Found variable %s \n",var->name);
      VarInfo* patchedVar=dictGet(patchVars,var->name);
      if(!patchedVar)
      {
        //todo: do we need to do anything special to handle removal of variables in the patch?
        printf("warning: var %s seems to have been removed in the patch\n",var->name);
        continue;
      }
      TypeInfo* ti1=var->type;
      TypeInfo* ti2=patchedVar->type;
      bool needsTransform=false;
      if(dictExists(trans->typeTransformers,ti1->name))
      {
        needsTransform=true;
      }
      else
      {
        //todo: should have some sort of caching for types we've already
        //determined to be equal
        TypeTransform* transform=NULL;
        if(!compareTypes(ti1,ti2,&transform))
        {
          if(!transform)
          {
            //todo: may not want to actually abort, may just want to issue
            //an error
            fprintf(stderr,"Error, cannot generate type transformation for variable %s\n",var->name);
            fflush(stderr);
            abort();
          }
          printf("generated type transformation for type %s\n",ti1->name);
          dictInsert(trans->typeTransformers,ti1->name,transform);
          needsTransform=true;
        }
      }
      if(needsTransform)
      {
        fixupVariable(*var,trans,pid,oldBinElfInfo);
      }
    }
    printf("completed all transformations for compilation unit %s\n",cu1->name);
    freeTransformationInfo(trans);
    free(vars1);
  }
}

int main(int argc,char** argv)
{
  if(argc<3)
  {
    die("Usage: katana -g [-o OUT_FILE] OLD_BINARY NEW_BINARY \n\tOr: katana -p PATCH_FILE PID");
  }
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
    die("Failed to init ELF library\n");
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
    //diffAndFixTypes(diPatchee,diPatched);
    endPtrace();
  }
  return 0;
}

