/*
  File: versioning.c
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
      fprintf(stderr,"%s does not exist. Is this a GNU/Linux system or other unix system with a Plan-9 style /proc filesystem? If it is, then the process may have exited",execPath);
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
