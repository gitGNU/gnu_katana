/*
  File: typepatch.c
  Author: James Oakley
  Project: Katana (preliminary work)
  Date: January 10
  Description: Preliminary patching program just for patching main_v0.c's structure Foo to have an extra field
               The first version of this program is going to be very simple. It will perform the following steps
               1. Load main_v0 with dwarfdump and determine where the variable bar is stored
               2. Find all references to the variable bar
               3. Attach to the running process with ptrace
               4. Make the process execute mmap to allocate space for a new data segment
               5. Copy over field1 and field2 from Foo bar into the new memory area and zero the added field
               6. Fixup all locations referring to the old address of bar with the new address and set the offset accordingly (should be able to get information for fixups from the rel.text section)
  Usage: typepatch PID
         PID is expected to be a running process build from main_v0.c
*/
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <libelf.h>
#include <gelf.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/user.h>
#ifdef __USE_MISC
#include <sys/mman.h>
#else
#define __USE_MISC
#include <sys/mman.h>
#undef __USE_MISC
#endif
#include <sys/syscall.h>

int pid;//pid of running process
Elf_Data* hashTableData=NULL;
Elf_Data* symTabData=NULL;
int symTabCount=-1;
Elf* e=NULL;
size_t sectionHdrStrTblIdx=-1;
size_t strTblIdx=-1;
Elf_Data* textRelocData=NULL;
int textRelocCount=-1;
Elf_Data* dataData=NULL;//the data section
int dataStart=-1;
Elf_Data* textData=NULL;
int textStart=-1;

typedef unsigned int uint;

typedef struct _List
{
  void* value;
  struct _List* next;
} List;

void continuePtrace();

List* getRelocationItemsFor(int symIdx)
{
  assert(textRelocData);
  List* result=NULL;
  List* cur=NULL;
  //todo: support RELA as well?
  for(int i=0;i<textRelocCount;i++)
  {
    GElf_Rel rel;
    gelf_getrel(textRelocData,i,&rel);
    //printf("found relocation item for symbol %u\n",(unsigned int)ELF64_R_SYM(rel.r_info));
    if(ELF64_R_SYM(rel.r_info)!=symIdx)
    {
      continue;
    }
    if(!result)
    {
      result=malloc(sizeof(List));
      assert(result);
      cur=result;
    }
    else
    {
      cur->next=malloc(sizeof(List));
      assert(cur->next);
      cur=cur->next;
    }
    cur->next=NULL;
    cur->value=malloc(sizeof(GElf_Rel));
    assert(cur->value);
    memcpy(cur->value,&rel,sizeof(GElf_Rel));
  }
  return result;
}


int getSymtabIdx(char* symbolName)
{
  //todo: need to consider endianness?
  assert(hashTableData);
  assert(symTabData);
  assert(strTblIdx>0);
  
  //Hash table only contains dynamic symbols, don't use it here
  /*
  int nbucket, nchain;
  memcpy(&nbucket,hashTableData->d_buf,sizeof(Elf32_Word));
  memcpy(&nchain,hashTableData->d_buf+sizeof(Elf32_Word),sizeof(Elf32_Word));
  printf("there are %i buckets, %i chain entries, and total size of %i\n",nbucket,nchain,hashTableData->d_size);


  unsigned long int hash=elf_hash(symbolName)%nbucket;
  printf("hash is %lu\n",hash);
  //get the index from the bucket
  int idx;
  memcpy(&idx,hashTableData->d_buf+sizeof(Elf32_Word)*(2+hash),sizeof(Elf32_Word));
  int nextIdx=idx;
  char* symname="";
  while(strcmp(symname,symbolName))
  {
    idx=nextIdx;
    printf("idx is %i\n",idx);
    if(STN_UNDEF==idx)
    {
      fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
      return STN_UNDEF;
    }
    GElf_Sym sym;
    
    if(!gelf_getsym(symTabData,idx,&sym))
    {
      fprintf(stderr,"Error getting symbol with idx %i from symbol table. Error is: %s\n",idx,elf_errmsg(-1));
      
    }
    symname=elf_strptr(e,strTblIdx,sym.st_name);
    //update index for the next go-around if need be from the chain table
    memcpy(&nextIdx,hashTableData->d_buf+sizeof(Elf32_Word)*(2+nbucket+idx),sizeof(Elf32_Word));
  }
  return idx;
  */
  
  //traverse the symbol table to find the symbol we're looking for. Yes this is slow
  //todo: build our own hash table since the .hash section seems incomplete
  for (int i = 0; i < symTabCount; ++i)
  {
    GElf_Sym sym;
    gelf_getsym(symTabData,i,&sym);
    char* symname=elf_strptr(e, strTblIdx, sym.st_name);
    if(!strcmp(symname,symbolName))
    {
      return i;
    }
  }
  fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
  return STN_UNDEF;
}

void printSymTab()
{
  printf("symbol table:\n");
  /* print the symbol names */
  for (int i = 0; i < symTabCount; ++i)
  {
    GElf_Sym sym;
    gelf_getsym(symTabData, i, &sym);
    printf("%i. %s\n", i,elf_strptr(e, strTblIdx, sym.st_name));
  }

}

void writeOut(char* outfname)
{
  //for some reason writing out to the same file descriptor isn't
  //working well, so we create a copy of all of the elf structures and write out to a new one
  int outfd = creat(outfname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", outfname);
  }
  //code inspired by ecp in elfutils tests
  Elf *outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
  gelf_newehdr (outelf, gelf_getclass (e));
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr=gelf_getehdr(e,&ehdr_mem);
  gelf_update_ehdr (outelf,ehdr);
  if(ehdr->e_phnum > 0)
  {
    int cnt;
    gelf_newphdr(outelf,ehdr->e_phnum);
    for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      gelf_update_phdr (outelf,cnt,gelf_getphdr(e,cnt,&phdr_mem));
    }
  }
  Elf_Scn* scn=NULL;
  while ((scn = elf_nextscn(e,scn)))
  {
    Elf_Scn *newscn = elf_newscn (outelf);
    GElf_Shdr shdr_mem;
    gelf_update_shdr(newscn,gelf_getshdr(scn,&shdr_mem));
    *elf_newdata(newscn) = *elf_getdata (scn,NULL);
  }
  elf_flagelf (outelf, ELF_C_SET, ELF_F_LAYOUT);

  if (elf_update (outelf, ELF_C_WRITE) <0)
  {
    fprintf(stdout,"Failed to write out elf file: %s\n",elf_errmsg (-1));
  }

  elf_end (outelf);
  close (outfd);
}

void startPtrace()
{
  if(ptrace(PTRACE_ATTACH,pid,NULL,NULL)<0)
  {
    fprintf(stderr,"ptrace failed to attach to process\n");
    exit(1);
  }
  //kill(pid,SIGSTOP);
  /*if(ptrace(PTRACE_CONT,pid,NULL,SIGSTOP)<0)
  {
    //just make sure the process is running
    perror("failed to continue process\n");
    exit(1);
    }*/

  //this line included on recommendation of phrack
  //http://phrack.org/issues.html?issue=59&id=8#article
  //I'm not entirely positive why
  //todo: figure this out
  waitpid(pid , NULL , WUNTRACED);
  printf("started ptrace\n");
}

void modifyTarget(long addr,uint value)
{
  if(ptrace(PTRACE_POKEDATA,pid,addr,value)<0)
  {
    perror("ptrace POKEDATA failed\n");
    exit(1);
  }
}

//copies numBytes from data to addr in target
void memcpyToTarget(long addr,uint* data,int numBytes)
{
  for(int i=0;i<numBytes;i++)
  {
    modifyTarget(addr+i,data[i]);
  }
}

//copies numBytes to data from addr in target
void memcpyFromTarget(uint* data,long addr,int numBytes)
{
  for(int i=0;i<numBytes;i++)
  {
    uint val=ptrace(PTRACE_PEEKDATA,pid,addr+i);
    if(errno)
    {
      perror("ptrace PEEKDATA failed\n");
      exit(1);
    }
    data[i]=val;
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
    exit(1);
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
    exit(1);
  }
  //and run the code
  continuePtrace();
  wait(NULL);
  if(ptrace(PTRACE_GETREGS,pid,NULL,&newRegs) < 0)
  {
    perror("ptrace getregs failed (getting regs after mmap syscall)\n");
    exit(1);
  }
  int retval=newRegs.eax;
  if((void*)retval==MAP_FAILED)
  {
    fprintf(stderr,"mmap in target failed\n");
    exit(1);
  }
  //restore the old registers
  if(ptrace(PTRACE_SETREGS,pid,NULL,&oldRegs)<0)
  {
    perror("ptrace setregs failed (when trying to resotre old registers)\n");
    exit(1);
  }
  return retval;
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
    exit(1);
  }
  kill(pid,SIGCONT);
}

void findELFSections()
{
  //todo: this is deprecated but the non deprecated version
  //(elf_getshdrstrndx) isn't linking on my system. This may betray some
  //more important screw-up somewhere)
  elf_getshstrndx (e, &sectionHdrStrTblIdx);
  
  for(Elf_Scn* scn=elf_nextscn (e, NULL);scn;scn=elf_nextscn(e,scn))
  {
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    char* name=elf_strptr(e,sectionHdrStrTblIdx,shdr.sh_name);
    if(!strcmp(".hash",name))
    {
      printf("found symbol hash table\n");
      hashTableData=elf_getdata(scn,NULL);
    }
    else if(!strcmp(".symtab",name))
    {
      symTabData=elf_getdata(scn,NULL);
      symTabCount = shdr.sh_size / shdr.sh_entsize;
      strTblIdx=shdr.sh_link;
    }
    else if(!strcmp(".strtab",name))
    {
      //todo: use this or get sh_link value from .symtab section
      // strTblIdx=elf_ndxscn(scn);
    }
    else if(!strcmp(".rel.text",name))
    {
      textRelocData=elf_getdata(scn,NULL);
      textRelocCount = shdr.sh_size / shdr.sh_entsize;
    }
    else if(!strcmp(".data",name))
    {
      dataData=elf_getdata(scn,NULL);
      dataStart=shdr.sh_addr;
      printf("data size if 0x%x at offset 0x%x\n",dataData->d_size,(uint)dataData->d_off);
      printf("data section starts at address 0x%x\n",dataStart);
    }
    else if(!strcmp(".text",name))
    {
      textData=elf_getdata(scn,NULL);
      textStart=shdr.sh_addr;
    }
  }
}

int main(int argc,char** argv)
{
  if(argc<2)
  {
    fprintf(stderr,"must specify pid to attach to\n");
    exit(1);
  }
  pid=atoi(argv[1]);
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    fprintf(stderr,"Failed to init ELF library\n");
    exit(1);
  }
  int fd=open("patchee",O_RDONLY);
  if(fd < 0)
  {
    fprintf(stderr,"Failed to open binary to be patched\n");
    exit(1);
  }
  e=elf_begin(fd,ELF_C_READ,NULL);
  if(!e)
  {
    fprintf(stderr,"Failed to open as an ELF file %s\n",elf_errmsg(-1));
    exit(1);
  }

  findELFSections();

  //printSymTab();
  int barSymIdx=getSymtabIdx("bar");
  GElf_Sym sym;
  gelf_getsym(symTabData,barSymIdx,&sym);
  int barAddress=sym.st_value;
  printf("bar located at %x\n",(unsigned int)barAddress);

  List* relocItems=getRelocationItemsFor(barSymIdx);
  startPtrace();

  //allocate a new page for us to put the new variable in
  long newPageAddr=mmapTarget(sysconf(_SC_PAGE_SIZE),PROT_READ|PROT_WRITE);
  printf("mapped in a new page at 0x%x\n",(uint)newPageAddr);

  //now set the data in that page
  //first get it from the old address
  uint barData[4];
  memcpyFromTarget(barData,barAddress,3);
  printf("read data %i,%i,%i\n",barData[0],barData[1],barData[2]);
  //copy it to the new address
  memcpyToTarget(newPageAddr,barData,3);
  
  for(List* li=relocItems;li;li=li->next)
  {
    GElf_Rel* rel=li->value;
    printf("relocation for bar at %x with type %i\n",(unsigned int)rel->r_offset,(unsigned int)ELF64_R_TYPE(rel->r_info));
    //modify the data to shift things over 4 bytes just to see if we can do it
    uint* oldAddr=textData->d_buf+(rel->r_offset-textStart);
    printf("old addr is ox%x\n",*oldAddr);
    uint newAddr=newPageAddr+(*oldAddr-barAddress);
    //*oldAddr=newAddr;
    modifyTarget(rel->r_offset,newAddr);
  }
  endPtrace();

  //flush modifications to disk
  //except not now, now trying to modify it in memory
  //writeOut("newpatchee");
  //all done
  elf_end(e);
  close(fd);
  return 0;
}

