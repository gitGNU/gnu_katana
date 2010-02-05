/*
  File: versioning.h
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: deal with patch versioning
*/

#ifndef versioning_h
#define versioning_h
#include "elfparse.h"
ElfInfo* getElfRepresentingProc(int pid);
int calculateVersionAfterPatch(int pid,ElfInfo* patch);
char* createKatanaDirs(int pid,int version);
#endif
