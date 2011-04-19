/*
  File: file.h
  Author: James Oakley
  Copyright (C): 2010 James Oakley
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version.

  This file was not written while under employment by Dartmouth
  College and the attribution requirements on the rest of Katana do
  not apply to code taken from this file.
  Project:  katana
  Date: January 2011
  Description:  Utility methods for files
*/


#include "file.h"
#include "logging.h"
#include "util.h"

//Note: reseeksfile to the beginning
int getFileLength(FILE* f)
{
  fseek(f,0,SEEK_END);
  int flen=ftell(f);
  fseek(f,0,SEEK_SET);
  return flen;
}

//the returned memory should be freed
char* getFileContents(char* filename,int* flen)
{
  FILE* f=fopen(filename,"r");
  if(!f)
  {
    logprintf(ELL_WARN,ELS_MISC,"Unable to open file %s for reading\n",filename);
    return NULL;
  }
  int len=getFileLength(f);
  char* buf=zmalloc(len);
  fread(buf,len,1,f);
  if(flen)
  {
    *flen=len;
  }
  fclose(f);
  return buf;
}
