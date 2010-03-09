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
//todo: does addr have to be aligned
void memcpyToTarget(long addr,byte* data,int numBytes);
//copies numBytes to data from addr in target
//todo: does addr have to be aligned
void memcpyFromTarget(byte* data,long addr,int numBytes);

//like memcpyFromTarget except doesn't kill katana
//if ptrace fails
//returns true if it succeseds
bool memcpyFromTargetNoDeath(byte* data,long addr,int numBytes);

void getTargetRegs(struct user_regs_struct* regs);
void setTargetRegs(struct user_regs_struct* regs);
//allocate a region of memory in the target
//return the address (in the target) of the region
//or NULL if the operation failed
addr_t mmapTarget(int size,int prot);

//must be called before any calls to mallocTarget
void setMallocAddress(addr_t addr);
//must be called before any calls to mallocTarget
void setTargetTextStart(addr_t addr);
addr_t mallocTarget(word_t len);

//compare a string to a string located
//at a certain address in the target
//return true if the strings match
//up to strlen(str) characters
bool strnmatchTarget(char* str,addr_t strInTarget);

void setBreakpoint(addr_t loc);
void removeBreakpoint(addr_t loc);
#endif
