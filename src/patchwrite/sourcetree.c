/*
  File: sourcetree.c
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
  Description: functions for dealing with the original and modified source trees
*/

#include "sourcetree.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "util/logging.h"
#include "util/path.h"
#include "elfcmp.h"

ElfInfo* getOriginalObject(ObjFileInfo* obj)
{
  ElfInfo* result=openELFFile(obj->pathToOriginal);
  findELFSections(result);
  return result;
}
ElfInfo* getModifiedObject(ObjFileInfo* obj)
{
  ElfInfo* result=openELFFile(obj->pathToModified);
  findELFSections(result);
  return result;
}

//returns non-zero for files ending in .o
int filterOnlyObjAndDirs(const struct dirent* d)
{
  if(DT_REG==d->d_type &&
     strlen(d->d_name)>2 &&
     !strcmp(".o",&d->d_name[strlen(d->d_name)-2]))
  {
    return 1;
  }
  else if(DT_DIR==d->d_type &&
          strcmp(".",d->d_name) &&
          strcmp("..",d->d_name))
  {
    return 1;
  }
  return 0;
}

//returns list of ObjFileInfo, will not include any with state EOS_UNCHANGED
List* getChangedObjectFilesInSourceTree(char* origSourceTree,char* modSourceTree)
{
  List* head=NULL;
  List* tail=NULL;
  struct stat s;
  if(origSourceTree && stat(origSourceTree,&s))
  {
    death("Original source tree at %s does not exist\n",origSourceTree);
  }
  else if(modSourceTree && stat(modSourceTree,&s))
  {
    death("Modified source tree at %s does not eist\n",modSourceTree);
  }
  struct dirent** dirEntriesOrig=NULL;
  int numEntriesOrig=0;
  if(origSourceTree)
  {
    numEntriesOrig=scandir(origSourceTree,&dirEntriesOrig,filterOnlyObjAndDirs,alphasort);
  }
  struct dirent** dirEntriesMod=NULL;
  int numEntriesMod=0;
  if(modSourceTree)
  {
    numEntriesMod=scandir(modSourceTree,&dirEntriesMod,filterOnlyObjAndDirs,alphasort);
  }
  int i=0;
  int j=0;
  for(;i<numEntriesOrig && j<numEntriesMod;)
  {
    assert(dirEntriesOrig[i]);
    assert(dirEntriesMod[j]);
    assert(dirEntriesOrig[i]->d_name);
    assert(dirEntriesMod[j]->d_name);
    char* fullPathOrig=joinPaths(origSourceTree,dirEntriesOrig[i]->d_name);
    char* fullPathMod=joinPaths(modSourceTree,dirEntriesMod[j]->d_name);
    int cmp=strcmp(dirEntriesOrig[i]->d_name,dirEntriesMod[j]->d_name);
    if(!cmp)
    {
      if(DT_DIR==dirEntriesOrig[i]->d_type && DT_DIR==dirEntriesMod[j]->d_type)
      {
        //recurse on the directories
        head=concatLists(head,tail,
                         getChangedObjectFilesInSourceTree(fullPathOrig,fullPathMod),
                         NULL,&tail);
        free(fullPathOrig);
        free(fullPathMod);
        free(dirEntriesOrig[i]);
        free(dirEntriesMod[j]);
        i++,j++;
        continue;
      }
      else if(DT_DIR==dirEntriesOrig[i]->d_type || DT_DIR==dirEntriesMod[j]->d_type)
      {
        death("Why are you giving one of your directories a .o extension? This parser isn't terribly smart, so it breaks!\n");
      }

      if(!elfcmp(fullPathOrig,fullPathMod))
      {
        logprintf(ELL_INFO_V1,ELS_SOURCETREE,"Object files %s and %s differ\n",fullPathOrig,fullPathMod);
                
        List* li=zmalloc(sizeof(List));
        ObjFileInfo* obj=zmalloc(sizeof(ObjFileInfo));
        obj->state=EOS_MODIFIED;
        obj->pathToOriginal=fullPathOrig;
        obj->pathToModified=fullPathMod;
        li->value=obj;
        listAppend(&head,&tail,li);
        free(dirEntriesOrig[i]);
        free(dirEntriesMod[j]);
        i++,j++;
      }
      else
      {
          logprintf(ELL_INFO_V1,ELS_SOURCETREE,"Object file %s does not differ between versions\n",fullPathOrig);
          free(fullPathOrig);
          free(fullPathMod);
          free(dirEntriesOrig[i]);
          free(dirEntriesMod[j]);
          i++,j++;
      }
    }
    else if(cmp<0)
    {
      if(DT_DIR==dirEntriesOrig[i]->d_type)
      {
        //recurse on the directories, only
        //the old version is present though
        head=concatLists(head,tail,
                         getChangedObjectFilesInSourceTree(fullPathOrig,NULL),
                         NULL,&tail);
      }
      else
      {
        logprintf(ELL_WARN,ELS_SOURCETREE,"An object file '%s' has been removed from the new version of the source\n",dirEntriesOrig[i]->d_name);
      }
      free(fullPathOrig);
      free(fullPathMod);
      free(dirEntriesOrig[i]);
      i++;

    }
    else if (cmp>0)
    {
      if(DT_DIR==dirEntriesOrig[i]->d_type)
      {
        //recurse on the directories, only
        //the old version is present though
        head=concatLists(head,tail,
                         getChangedObjectFilesInSourceTree(NULL,fullPathMod),
                         NULL,&tail);
      }
      else
      {
        
        //file only exists in new version
        List* li=zmalloc(sizeof(List));
        ObjFileInfo* obj=zmalloc(sizeof(ObjFileInfo));
        obj->state=EOS_NEW;
        obj->pathToModified=fullPathMod;
        li->value=obj;
        listAppend(&head,&tail,li);
      }
      free(fullPathOrig);
      free(dirEntriesMod[j]);
      j++;
    }
  }

  
  for(;i<numEntriesOrig;i++)
  {
    char* fullPathOrig=joinPaths(origSourceTree,dirEntriesOrig[i]->d_name);
    logprintf(ELL_INFO_V1,ELS_SOURCETREE,"Object file %s does not exist in the new version\n",fullPathOrig);
  }
  for(;j<numEntriesMod;j++)
  {
    char* fullPathMod=joinPaths(modSourceTree,dirEntriesMod[i]->d_name);
    //file only exists in new version
    List* li=zmalloc(sizeof(List));
    ObjFileInfo* obj=zmalloc(sizeof(ObjFileInfo));
    obj->state=EOS_NEW;
    obj->pathToModified=fullPathMod;
    li->value=obj;
    listAppend(&head,&tail,li);
  }
  free(dirEntriesOrig);
  if(dirEntriesMod)
  {
    free(dirEntriesMod);
  }
  
  return head;
}

void deleteObjFileInfo(ObjFileInfo* obj)
{
  free(obj->pathToModified);
  free(obj->pathToOriginal);
  free(obj);
}
