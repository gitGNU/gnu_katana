/*
  File: sourcetree.c
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: functions for dealing with the original and modified source trees
*/

#include "sourcetree.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include "util/logging.h"
#include "util/path.h"

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
    char* fullPathOrig=joinPaths(origSourceTree,dirEntriesOrig[i]->d_name);
    char* fullPathMod=joinPaths(modSourceTree,dirEntriesMod[i]->d_name);
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
      //obj file exists in both of them
      //compare them for equality
      if(system("which elfcmp > /dev/null"))
      {
        death("You must have the elfcmp program installed. You can obtain this from elfutils which can be found at https://fedorahosted.org/elfutils/\n");
      }

      char* buf=zmalloc(strlen(fullPathOrig)+strlen(fullPathMod)+32);
      sprintf(buf,"elfcmp %s %s",fullPathOrig,fullPathMod);
      if(system(buf))
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
      free(buf);
    }
    else if(cmp<0)
    {
      logprintf(ELL_WARN,ELS_SOURCETREE,"An object file '%s' has been removed from the new version of the source\n",dirEntriesOrig[i]->d_name);
      free(fullPathOrig);
      free(fullPathMod);
      free(dirEntriesOrig[i]);
      i++;
    }
    else if (cmp>0)
    {
      //file only exists in new version
      List* li=zmalloc(sizeof(List));
      ObjFileInfo* obj=zmalloc(sizeof(ObjFileInfo));
      obj->state=EOS_NEW;
      obj->pathToModified=fullPathMod;
      li->value=obj;
      listAppend(&head,&tail,li);
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
