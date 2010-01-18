/*
  File: elfparse.c
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: Read information from an ELF file
*/

#include "elfparse.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

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
int fd=-1;//file descriptor for elf file

Elf* openELFFile(char* fname)
{
  fd=open(fname,O_RDONLY);
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
  return e;
}

void endELF(Elf* _e)
{
  //bad programming practice with globals here
  //todo: fix it
  assert(_e==e);
  elf_end(e);
  close(fd);
}


addr_t getSymAddress(int symIdx)
{
  GElf_Sym sym;
  gelf_getsym(symTabData,symIdx,&sym);
  return sym.st_value;
}

word_t getTextAtRelOffset(int offset)
{
  return *((word_t*)(textData->d_buf+(offset-textStart)));
}

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



void findELFSections()
{
  //todo: this is deprecated but the non deprecated version
  //(elf_getshdrstrndx) isn't linking on my system. This may betray some
  //more important screw-up somewhere)
  elf_getshdrstrndx (e, &sectionHdrStrTblIdx);
  
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
      //printf("data size is 0x%x at offset 0x%x\n",dataData->d_size,(uint)dataData->d_off);
      //printf("data section starts at address 0x%x\n",dataStart);
    }
    else if(!strcmp(".text",name))
    {
      textData=elf_getdata(scn,NULL);
      textStart=shdr.sh_addr;
    }
  }
}

