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
#include <sys/stat.h>
#include "logging.h"
#include "path.h"
#include <limits.h>
#include <unistd.h>

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

//path is made relative to ref
char* makePathRelativeTo(char* path,char* ref)
{
  char* path_=absPath(path);
  char* ref_=absPath(ref);
  if(!strncmp(path_,ref_,strlen(ref_)))
  {
    char* result=strdup(path_+strlen(ref_));
    free(path_);
    free(ref_);
    return result;
  }
  else
  {
    death("Need to implement more compilated forms of makePathRelativeTo. Poke James\n");
    return NULL;
  }
}

//return the directory portion of path
//the result should be freed
char* getDirectoryOfPath(char* path)
{
  struct stat s;
  if(0!=stat(path,&s))
  {
    logprintf(ELL_WARN,ELS_PATH,"path %s does not exist, cannot determine the directory portion\n",path);
  }
  if(S_ISDIR(s.st_mode))
  {
    return strdup(path);//the whole thing is the directory portion
  }
  else
  {
    char* lastSlash=strrchr(path,'/');
    if(!lastSlash)
    {
      return strdup("");
    }
    char* result=strdup(path);
    result[lastSlash-path+1]='\0';
    return result;
  }
}

//The returned string should be freed
char* absPath(char* path)
{
  return realpath(path,NULL);
}

bool isAbsPath(char* path)
{
  return path[0]=='/';
}
