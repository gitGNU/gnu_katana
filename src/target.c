/*
  File: target.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description:  low-level functions for modifying an in-memory target
*/

#include "target.h"
#include <sys/ptrace.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include "util.h"
#include "types.h"
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <sys/mman.h>




//todo: bad programming
extern int pid;

void startPtrace()
{
  if(ptrace(PTRACE_ATTACH,pid,NULL,NULL)<0)
  {
    fprintf(stderr,"ptrace failed to attach to process\n");
    die(NULL);
  }
  //kill(pid,SIGSTOP);
  /*if(ptrace(PTRACE_CONT,pid,NULL,SIGSTOP)<0)
  {
    //just make sure the process is running
    perror("failed to continue process\n");
    die(NULL);
    }*/

  //this line included on recommendation of phrack
  //http://phrack.org/issues.html?issue=59&id=8#article
  //I'm not entirely positive why
  //todo: figure this out
  waitpid(pid , NULL , WUNTRACED);
  printf("started ptrace\n");
}

void continuePtrace()
{
  if((ptrace(PTRACE_CONT , pid , NULL , NULL)) < 0)
  {
    perror("ptrace cont failed");
    exit(-1);
  }
  //this while loop idea courtesy of Phrack at
  // http://phrack.org/issues.html?issue=59&id=8#article
  //seems to cause issues with mmapTarget though, so
  //we won't use it for now
  //todo: figure out its purpose and if it's necessary
  //int s;
	//while (!WIFSTOPPED(s)) waitpid(pid , &s , WNOHANG);

}

void endPtrace()
{
  
  if(ptrace(PTRACE_DETACH,pid,NULL,NULL)<0)
  {
    fprintf(stderr,"ptrace failed to detach\n");
    die(NULL);
  }
  kill(pid,SIGCONT);
}


void modifyTarget(addr_t addr,uint value)
{
  if(ptrace(PTRACE_POKEDATA,pid,addr,value)<0)
  {
    perror("ptrace POKEDATA failed\n");
    die(NULL);
  }
}

//todo: figure this out on x86
#define PTRACE_WORD_SIZE 4


//copies numBytes from data to addr in target
void memcpyToTarget(long addr,char* data,int numBytes)
{
  for(int i=0;i<numBytes;i+=PTRACE_WORD_SIZE)
  {
    if(i+PTRACE_WORD_SIZE<=numBytes)
    {
      modifyTarget(addr+i,data[i]);
    }
    else
    {
      assert(sizeof(uint)==PTRACE_WORD_SIZE);
      uint tmp=0;
      memcpy(&tmp,data+i,numBytes-i);
      modifyTarget(addr+i,tmp);
    }
  }
}


//copies numBytes to data from addr in target
void memcpyFromTarget(char* data,long addr,int numBytes)
{
  for(int i=0;i<numBytes;i+=4)
  {
    uint val=ptrace(PTRACE_PEEKDATA,pid,addr+i);
    if(errno)
    {
      perror("ptrace PEEKDATA failed\n");
      die(NULL);
    }
    if(i+PTRACE_WORD_SIZE<=numBytes)
    {
      memcpy(data+i,&val,PTRACE_WORD_SIZE);
    }
    else
    {
      //at the end and wasn't aligned
      memcpy(data+i,&val,numBytes-i);
    }
  }
}

//allocate a region of memory in the target
//return the address (in the target) of the region
//or NULL if the operation failed
long mmapTarget(int size,int prot)
{
  printf("requesting mmap of page of size %i\n",size);
  //map code influenced by code from livepatch
  //http://ukai.jp/Software/livepatch

  //0xcd indicates a trap
  //0x80 indicates that this trap is a syscall
  //0x0cc is int3 instruction, will cause the target
  //to halt execution until the controlling process calls wait
  char code[]={0xcd,0x80,0xcc,0x00};

  struct user_regs_struct oldRegs,newRegs;
  if(ptrace(PTRACE_GETREGS,pid,NULL,&oldRegs) < 0)
  {
    perror("ptrace getregs failed\n");
    die(NULL);
  }
  newRegs=oldRegs;
  //we're going to throw the parameters to this syscall
  //as well as the code itself on the stack
  //note that this approach requires an executable stack.
  //if we didn't have an executable stack we could temporarily replace
  //code in the text segment at the program counter
  newRegs.esp-=sizeof(int);
  long code4Bytes;
  assert(sizeof(code4Bytes)==4);
  memcpy(&code4Bytes,code,4);
  modifyTarget(newRegs.esp,code4Bytes);
  newRegs.eip=newRegs.esp;//we put our code on the stack, so direct the pc to it
  long returnAddr=newRegs.esp+2;//the int3 instruction
  
  //the call we want to make is
  //mmap(NULL,size,prot,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)
  //mmap in libc is just a wrapper over a kernel call
  //we have a lot to put on the stack
  modifyTarget(newRegs.esp-=4,0);
  modifyTarget(newRegs.esp-=4,-1);
  modifyTarget(newRegs.esp-=4,MAP_PRIVATE|MAP_ANONYMOUS);
  modifyTarget(newRegs.esp-=4,prot);
  modifyTarget(newRegs.esp-=4,size);
  modifyTarget(newRegs.esp-=4,(int)NULL);
  modifyTarget(newRegs.esp-=4,returnAddr);
  newRegs.ebx=newRegs.esp+4;//syscall, takes arguments in registers,
                            //this is a pointer to the arguments on the stack
  newRegs.eax=SYS_mmap;//syscall number to identify that this is an mmap call
  
  //now actually tell the process about these registers
  if(ptrace(PTRACE_SETREGS,pid,NULL,&newRegs)<0)
  {
    perror("ptrace setregs failed\n");
    die(NULL);
  }
  //and run the code
  continuePtrace();
  wait(NULL);
  if(ptrace(PTRACE_GETREGS,pid,NULL,&newRegs) < 0)
  {
    perror("ptrace getregs failed (getting regs after mmap syscall)\n");
    die(NULL);
  }
  int retval=newRegs.eax;
  if((void*)retval==MAP_FAILED)
  {
    fprintf(stderr,"mmap in target failed\n");
    die(NULL);
  }
  //restore the old registers
  if(ptrace(PTRACE_SETREGS,pid,NULL,&oldRegs)<0)
  {
    perror("ptrace setregs failed (when trying to resotre old registers)\n");
    die(NULL);
  }
  return retval;
}

