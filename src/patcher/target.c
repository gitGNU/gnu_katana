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
#include "types.h"
#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>
#include <sys/mman.h>
#include "config.h"
#include "util/logging.h"


//todo: bad programming
extern int pid;
addr_t mallocPLTAddress=0;

void setMallocPLTAddress(addr_t addr)
{
  mallocPLTAddress=addr;
}

void startPtrace()
{
  if(ptrace(PTRACE_ATTACH,pid,NULL,NULL)<0)
  {
    fprintf(stderr,"ptrace failed to attach to process\n");
    death(NULL);
  }
  //kill(pid,SIGSTOP);
  /*if(ptrace(PTRACE_CONT,pid,NULL,SIGSTOP)<0)
  {
    //just make sure the process is running
    perror("failed to continue process\n");
    death(NULL);
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
    death(NULL);
  }
  kill(pid,SIGCONT);
}


void modifyTarget(addr_t addr,word_t value)
{
  logprintf(ELL_INFO_V2,ELS_HOTPATCH,"Trying to poke data at 0x%x with value 0x%x\n",(uint)addr,(uint)value);
  if(ptrace(PTRACE_POKEDATA,pid,addr,value)<0)
  {
    logprintf(ELL_ERR,ELS_HOTPATCH,"Failed to poke data at 0x%x with value 0x%x\n",(uint)addr,(uint)value);
    perror("ptrace POKEDATA failed in modifyTarget\n");
    death(NULL);
  }
  //test to make sure the write went through
  if(isFlag(EKCF_CHECK_PTRACE_WRITES))
  {
    uint val=ptrace(PTRACE_PEEKDATA,pid,addr);
    if(errno)
    {
      perror("ptrace PEEKDATA failed\n");
      death(NULL);
    }
    if(val!=value)
    {
      death("modifyTarget failed, failed to validate result of write\n");
    }
    else
    {
      logprintf(ELL_INFO_V4,ELS_HOTPATCH,"modifyTarget validated write\n");
    }
  }
}

//todo: figure this out on x86
#define PTRACE_WORD_SIZE 4

//todo: look more into this. ptrace
//man page says it's required but in practice doesn't seem to be
#define require_ptrace_alignment

//copies numBytes from data to addr in target
void memcpyToTarget(long addr,byte* data,int numBytes)
{
  #ifdef require_ptrace_alignment
  //ptrace requires all addresses to be word-aligned
  addr_t misalignment=addr%PTRACE_WORD_SIZE;
  if(misalignment)
  {
    logprintf(ELL_INFO_V4,ELS_HOTPATCH,"misalignment is %i, addr is 0x%x\n",misalignment,addr);
    byte firstWord[PTRACE_WORD_SIZE];
    assert(PTRACE_WORD_SIZE==sizeof(word_t));
    //we'll be copying back a few bytes that already existed
    memcpyFromTarget(firstWord,addr-misalignment,PTRACE_WORD_SIZE);
    logprintf(ELL_INFO_V4,ELS_HOTPATCH,"copied bytes {0x%x,0x%x,0x%x,0%x} from 0x%x\n",(uint)firstWord[0],(uint)firstWord[1],(uint)firstWord[2],(uint)firstWord[3],(uint)(addr-misalignment));
    int bytesInWd=PTRACE_WORD_SIZE-misalignment;
    logprintf(ELL_INFO_V4,ELS_HOTPATCH,"copying in %i patch bytes in first wd\n",bytesInWd);
    memcpy(&firstWord[misalignment],data,bytesInWd);
    logprintf(ELL_INFO_V4,ELS_HOTPATCH,"now copying bytes {0x%x,0x%x,0x%x,0x%x} to 0x%x\n",(uint)firstWord[0],(uint)firstWord[1],(uint)firstWord[2],(uint)firstWord[3],addr-misalignment);
    word_t wd;
    memcpy(&wd,firstWord,sizeof(word_t));
    modifyTarget(addr-misalignment,wd);
    data+=bytesInWd;
    numBytes-=bytesInWd;
    addr+=bytesInWd;
    logprintf(ELL_INFO_V4,ELS_HOTPATCH,"addr is now 0x%x\n",addr);
    //now we're all set to carry on copying normally from an aligned address
  }

  assert(0==addr%PTRACE_WORD_SIZE);
  #endif

  
  for(int i=0;i<numBytes;i+=PTRACE_WORD_SIZE)
  {
    if(i+PTRACE_WORD_SIZE<=numBytes)
    {
      word_t val;
      memcpy(&val,data+i,4);
      modifyTarget(addr+i,val);
    }
    else
    {
      assert(sizeof(word_t)==PTRACE_WORD_SIZE);
      word_t tmp=0;
      memcpyFromTarget((byte*)&tmp,addr+i,sizeof(word_t));
      memcpy(&tmp,data+i,numBytes-i);
      modifyTarget(addr+i,tmp);
    }
  }
}

//like memcpyFromTarget except doesn't kill katana
//if ptrace fails
//returns true if it succeseds
bool memcpyFromTargetNoDeath(byte* data,long addr,int numBytes)
{
  logprintf(ELL_INFO_V4,ELS_HOTPATCH,"memcpyFromTarget: getting %i bytes from 0x%x\n",numBytes,(uint)addr);
  //todo: force address to be aligned
  for(int i=0;i<numBytes;i+=4)
  {
    uint val=ptrace(PTRACE_PEEKDATA,pid,addr+i);
    if(errno)
    {
      return false;
    }
    if(i+PTRACE_WORD_SIZE<=numBytes)
    {
      memcpy(data+i,&val,PTRACE_WORD_SIZE);
    }
    else
    {
      logprintf(ELL_INFO_V4,ELS_HOTPATCH,"memcpyFromTarget: wasn't aligned\n");
      //at the end and wasn't aligned
      memcpy(data+i,&val,numBytes-i);
    }
  }
  return true;
}

//copies numBytes to data from addr in target
void memcpyFromTarget(byte* data,long addr,int numBytes)
{
  if(!memcpyFromTargetNoDeath(data,addr,numBytes))
  {
    perror("ptrace PEEKDATA failed ");
    death(NULL);
  }      
}

void getTargetRegs(struct user_regs_struct* regs)
{
  if(ptrace(PTRACE_GETREGS,pid,NULL,regs) < 0)
  {
    perror("ptrace getregs failed\n");
    death(NULL);
  }
}

void setTargetRegs(struct user_regs_struct* regs)
{
  if(ptrace(PTRACE_SETREGS,pid,NULL,regs)<0)
  {
    perror("ptrace setregs failed\n");
    death(NULL);
  }
}

//allocate a region of memory in the target using malloc
//should be used for when creating objects to be used in the program,
//as opposed to mmapTarget which should be used when mapping in new sections
//inspiration for some of this code came from
//http://www.hick.org/code/skape/papers/needle.txt
addr_t mallocTarget(word_t len)
{
  struct user_regs_struct oldRegs,newRegs;
  getTargetRegs(&oldRegs);
  getTargetRegs(&oldRegs);
  newRegs=oldRegs;

  //so we have to do two things
  //1. we have to push the amount to malloc onto the stack (5-byte instr)
  //2. we have to call malloc (5-byte instr).
  //  This is tricky because we have to know where
  //  malloc lives. We're going to have to find it in the PLT. That's
  //  outside the scope of this module though. We rely on it to be set
  //  from outside the module using the setMallocPLTAddress method
  //3. we have to restore the stack (3 byte instr)
  if(!mallocPLTAddress)
  {
    death("location of malloc is unknown\n");
  }

  //todo: diff for 64 bit
  //68 xx xx xx xx pushes amount to malloc onto the stack
  //e8 xx xx xx xx calls malloc
  //83 c4 04       adds 4 to esp (popping the stack without storing anywhere)
  #define codeLen 13
  byte code[codeLen]={0x68,0x00,0x00,0x00,0x00,
                      0xe8,0x00,0x00,0x00,0x00,
                      0x83,0xc4, 0x04};
  memcpy(code+1,&len,sizeof(word_t));
  addr_t relativeMallocPTAddr=mallocPLTAddress-newRegs.eip-10;
  memcpy(code+6,&relativeMallocPTAddr,sizeof(addr_t));
  byte oldText[13];
  memcpyFromTarget(oldText,newRegs.eip,codeLen);
  memcpyToTarget(newRegs.eip,code,codeLen);
  
  //and run the code
  continuePtrace();
  wait(NULL);
  getTargetRegs(&newRegs);//get the return value from the syscall
  int retval=newRegs.eax;
  printf("retval is 0x%x\n",(uint)retval);
  if((void*)retval==NULL)
  {
    death("malloc in target of size %i failed\n",len);
  }
  //printf("now at eip 0x%x\n",newRegs.eip);
  //restore the old code
  memcpyToTarget(oldRegs.eip,oldText,codeLen);
  //restore the old registers
  setTargetRegs(&oldRegs);
  return retval;
}

//allocate a region of memory in the target
//return the address (in the target) of the region
//or NULL if the operation failed
addr_t mmapTarget(int size,int prot)
{
  printf("requesting mmap of page of size %i\n",size);
  //map code influenced by code from livepatch
  //http://ukai.jp/Software/livepatch

  //0xcd indicates a trap
  //0x80 indicates that this trap is a syscall
  //0x0cc is int3 instruction, will cause the target
  //to halt execution until the controlling process calls wait
  byte code[]={0xcd,0x80,0xcc,0x00};

  struct user_regs_struct oldRegs,newRegs;
  getTargetRegs(&oldRegs);
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
  //printf("inserting code at eip 0x%x\n",newRegs.eip);
  byte oldText[4];
  memcpyFromTarget(oldText,newRegs.eip,4);
  modifyTarget(newRegs.eip,code4Bytes);
  printf("inserted syscall call\n");
  long returnAddr=newRegs.eip+2;//the int3 instruction

  
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
  printf("%x\n",SYS_mmap);
  printf("inserted syscall params on stack\n");
  
  newRegs.eax=SYS_mmap;//syscall number to identify that this is an mmap call
  //now actually tell the process about these registers
  setTargetRegs(&newRegs);
  
  //and run the code
  continuePtrace();
  wait(NULL);
  getTargetRegs(&newRegs);//get the return value from the syscall
  int retval=newRegs.eax;
  printf("retval is 0x%x\n",(uint)retval);
  if((void*)retval==MAP_FAILED)
  {
    fprintf(stderr,"mmap in target failed\n");
    death(NULL);
  }
  //printf("now at eip 0x%x\n",newRegs.eip);
  #ifndef OLD_MMAP_TARGET
  //restore the old code
  memcpyToTarget(oldRegs.eip,oldText,4);
  #endif
  //restore the old registers
  setTargetRegs(&oldRegs);
  printf("mmapped in new page successfully\n");
  return retval;
}


//compare a string to a string located
//at a certain address in the target
//return true if the strings match up to strlen(str) characters
bool strnmatchTarget(char* str,addr_t strInTarget)
{
  //todo: need robust error checking to make sure not
  //accessing memory out of bounds
  int len=strlen(str);
  char* buf=zmalloc(len+1);
  memcpyFromTarget((byte*)buf,strInTarget,len+1);
  buf[len]=0;
  return !strcmp(buf,str);
}
