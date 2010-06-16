/*
  File: versioning.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project: Katana
  Date: February 2010
  Description: Deal with patch versioning
*/
#include "versioning.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

ElfInfo* getElfRepresentingProc(int pid)
{
  ElfInfo* result=NULL;
  struct stat s;
  char previousPatchesDir[256];
  snprintf(previousPatchesDir,256,"/tmp/katana-%s/patched/%i",
           getenv("USER"),pid);
  //todo: enable this when actually supporting multiple versions
  //of patches
  if(0==stat(previousPatchesDir,&s) && false)
  {
    //this directory exists, there are previous patches
    struct dirent** dirEntries;
    int numEntries=scandir(previousPatchesDir,&dirEntries,NULL,versionsort);
    //we only care about the newest version, i.e the highest version number
    char* vname=strdup(dirEntries[numEntries-1]->d_name);
    free(dirEntries);
    char buf[256];
    snprintf(buf,256,"%s/%s/exe",previousPatchesDir,vname);
    result=openELFFile(buf);
  }
  else
  {
    char execPath[128];
    snprintf(execPath,128,"/proc/%i/exe",pid);
    struct stat s;
    if(0!=stat(execPath,&s))
    {
      fprintf(stderr,"%s does not exist. Is this a linux system or other unix system with a Plan-9 style /proc filesystem? If it is, then the process may have exited",execPath);
      death(NULL);
    }
    result=openELFFile(execPath);
  }
  //todo: handle the case where the written elf file
  //is corrupted. Try to detect that
  findELFSections(result);
  return result;
}

char* createKatanaDirs(int pid,int version)
{
  //todo: error checking
  char buf1[128];
  snprintf(buf1,128,"/tmp/katana-%s",getenv("USER"));
  mode_t mode=S_IRWXU;
  mkdir(buf1,mode);
  char buf2[256];
  snprintf(buf2,256,"%s/patched/",buf1);
  mkdir(buf2,mode);
  snprintf(buf2,256,"%s/patched/%i",buf1,pid);
  mkdir(buf2,mode);
  snprintf(buf2,256,"%s/patched/%i/%i",buf1,pid,version);
  mkdir(buf2,mode);
  return strdup(buf2);
}

int calculateVersionAfterPatch(int pid,ElfInfo* patch)
{
  //todo: versioning (hardcoded patching to version 1 here)
  return 1;
}

char* getVersionStringOfPatchSections()
{
  //todo: figure out an actual versioning system
  return "new";
}
