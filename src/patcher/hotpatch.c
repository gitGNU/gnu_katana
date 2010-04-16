/*
  File: hotpatch.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: functions for actually modifying the target and performing the hotpatching
*/
#include <assert.h>
#include "target.h"
#include "elfparse.h"
#include <unistd.h>
#include "hotpatch.h"
#include "relocation.h"
#include "symbol.h"
#include <math.h>

addr_t addrFreeSpace;
addr_t freeSpaceLeft;



int getIdxForField(TypeInfo* type,char* name)
{
  for(int i=0;i<type->numFields;i++)
  {
    if(!strcmp(name,type->fields[i]))
    {
      return i;
    }
  }
  return FIELD_DELETED;
}

//mmap some contiguous space in the target, but don't
//assume it's being used right now. It will be claimed
//by later calls to getFreeSpaceInTarget
void reserveFreeSpaceInTarget(uint howMuch)
{
  if(howMuch<=freeSpaceLeft)
  {
    return;
  }
  uint numPages=(uint)ceil((double)howMuch/(double)sysconf(_SC_PAGE_SIZE));
  //todo: separate out different function/structure for executable code
  addrFreeSpace=mmapTarget(numPages*sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE|PROT_EXEC);
  freeSpaceLeft=numPages*sysconf(_SC_PAGE_SIZE);
  //todo: if there's a little bit of free space left,
  //we just discard it. This is wasteful
}


addr_t getFreeSpaceInTarget(uint howMuch)
{
  addr_t retval;
  if(howMuch<=freeSpaceLeft)
  {
    retval=addrFreeSpace;
  }
  reserveFreeSpaceInTarget(howMuch);
  retval=addrFreeSpace;
  freeSpaceLeft-=howMuch;
  addrFreeSpace+=howMuch;
  return retval;
  //todo: if there's a little bit of free space left,
  //we just discard it. This is wasteful
}
