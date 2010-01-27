/*
  File: target.h
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: prototypes for functions to operate on an in-memory target
*/

#ifndef target_h
#define target_h
#include <sys/user.h>
#ifdef __USE_MISC
#include <sys/mman.h>
#else
#define __USE_MISC
#include <sys/mman.h>
#undef __USE_MISC
#endif
#include <sys/syscall.h>
#include "types.h"
//#include <sys/user.h>

void startPtrace();
void continuePtrace();
void endPtrace();
void modifyTarget(addr_t addr,uint value);
//copies numBytes from data to addr in target
void memcpyToTarget(long addr,char* data,int numBytes);
//copies numBytes to data from addr in target
void memcpyFromTarget(char* data,long addr,int numBytes);
void getTargetRegs(struct user_regs_struct* regs);
void setTargetRegs(struct user_regs_struct* regs);
//allocate a region of memory in the target
//return the address (in the target) of the region
//or NULL if the operation failed
long mmapTarget(int size,int prot);

#endif
