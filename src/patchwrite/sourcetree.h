/*
  File: sourcetree.c
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: functions for dealing with the original and modified source trees
*/

#ifndef sourcetree_h
#define sourcetree_h

#include "util/list.h"
#include "elfparse.h"

//use EOS prefix instead of EOFS because EOFS sounds like a filesystem
typedef enum
{
  EOS_MODIFIED,
  EOS_UNCHANGED,
  EOS_NEW
} E_OBJFILE_STATE;

typedef struct 
{
  E_OBJFILE_STATE state;
  char* pathToOriginal;
  char* pathToModified;
} ObjFileInfo;

//returns list of ObjFileInfo, will not include any with state EOS_UNCHANGED
List* getChangedObjectFilesInSourceTree(char* origSourceTree,char* modSourceTree);
ElfInfo* getOriginalObject(ObjFileInfo* obj);
ElfInfo* getModifiedObject(ObjFileInfo* obj);
#endif
