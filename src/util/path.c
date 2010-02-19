/*
  File: path.c
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: Functions for dealing with filepaths
*/

#include <string.h>
#include <stdbool.h>
#include "util.h"

bool strEndsWith(char* str,char* suffix)
{
  int suffixLen=strlen(suffix);
  int strLen=strlen(str);
  if(suffixLen > strLen)
  {return false;}
  int start=strLen-suffixLen;
  return !strcmp(str+start,suffix);
}


char* joinPaths(char* path1,char* path2)
{
  if(!path1 && !path2)
  {return NULL;}
  char* result=zmalloc(strlen(path1)+strlen(path2)+2);
  if(path1)
  {
    strcpy(result,path1);
  }
  if(!path2)
  {
    return result;
  }
  if(path1 && !strEndsWith(path1,"/"))
  {
    strcat(result,"/");
  }
  if(path2)
  {
    strcat(result,path2);
  }
  return result;
}
