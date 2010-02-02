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

ElfInfo* openELFFile(char* fname)
{
  ElfInfo* e=zmalloc(sizeof(ElfInfo));
  e->fd=open(fname,O_RDONLY);
  if(e->fd < 0)
  {
    char buf[128];
    snprintf(buf,128,"Failed to open elf file %s (open returned invalid fd)\n",fname);
    die(buf);
  }
  e->e=elf_begin(e->fd,ELF_C_READ,NULL);
  if(!e->e)
  {
    fprintf(stderr,"Failed to open as an ELF file %s\n",elf_errmsg(-1));
    free(e);
    die(NULL);
  }
  return e;
}

void endELF(ElfInfo* e)
{
  elf_end(e->e);
  close(e->fd);
  free(e);
}

bool getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym)
{
  assert(e->symTabData);
  return gelf_getsym(e->symTabData,symIdx,outSym);
}

addr_t getSymAddress(ElfInfo* e,int symIdx)
{
  GElf_Sym sym;
  gelf_getsym(e->symTabData,symIdx,&sym);
  return sym.st_value;
}

word_t getTextAtRelOffset(ElfInfo* e,int offset)
{
  return *((word_t*)(e->textData->d_buf+(offset-e->textStart)));
}

List* getRelocationItemsFor(ElfInfo* e,int symIdx)
{
  assert(e->textRelocData);
  List* result=NULL;
  List* cur=NULL;
  //todo: support RELA as well?
  for(int i=0;i<e->textRelocCount;i++)
  {
    GElf_Rel rel;
    gelf_getrel(e->textRelocData,i,&rel);
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


int getSymtabIdx(ElfInfo* e,char* symbolName)
{
  //todo: need to consider endianness?
  assert(e->hashTableData);
  assert(e->symTabData);
  assert(e->strTblIdx>0);
  
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
  for (int i = 0; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    gelf_getsym(e->symTabData,i,&sym);
    char* symname=elf_strptr(e->e, e->strTblIdx, sym.st_name);
    if(!strcmp(symname,symbolName))
    {
      return i;
    }
  }
  fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
  return STN_UNDEF;
}

void printSymTab(ElfInfo* e)
{
  printf("symbol table:\n");
  /* print the symbol names */
  for (int i = 0; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    gelf_getsym(e->symTabData, i, &sym);
    printf("%i. %s\n", i,elf_strptr(e->e, e->strTblIdx, sym.st_name));
  }

}

void writeOut(ElfInfo* e,char* outfname)
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
  gelf_newehdr (outelf, gelf_getclass(e->e));
  GElf_Ehdr ehdr_mem;
  GElf_Ehdr *ehdr=gelf_getehdr(e->e,&ehdr_mem);
  gelf_update_ehdr (outelf,ehdr);
  if(ehdr->e_phnum > 0)
  {
    int cnt;
    gelf_newphdr(outelf,ehdr->e_phnum);
    for (cnt = 0; cnt < ehdr->e_phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      gelf_update_phdr(outelf,cnt,gelf_getphdr(e->e,cnt,&phdr_mem));
    }
  }
  Elf_Scn* scn=NULL;
  while ((scn = elf_nextscn(e->e,scn)))
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



void findELFSections(ElfInfo* e)
{
  //todo: this is deprecated but the non deprecated version
  //(elf_getshdrstrndx) isn't linking on my system. This may betray some
  //more important screw-up somewhere)
  elf_getshdrstrndx(e->e, &e->sectionHdrStrTblIdx);
  
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    char* name=elf_strptr(e->e,e->sectionHdrStrTblIdx,shdr.sh_name);
    if(!strcmp(".hash",name))
    {
      printf("found symbol hash table\n");
      e->hashTableData=elf_getdata(scn,NULL);
    }
    else if(!strcmp(".symtab",name))
    {
      e->symTabData=elf_getdata(scn,NULL);
      e->symTabCount = shdr.sh_size / shdr.sh_entsize;
      e->strTblIdx=shdr.sh_link;
    }
    else if(!strcmp(".strtab",name))
    {
      //todo: use this or get sh_link value from .symtab section
      // strTblIdx=elf_ndxscn(scn);
    }
    else if(!strcmp(".rel.text",name))
    {
      e->textRelocData=elf_getdata(scn,NULL);
      e->textRelocCount = shdr.sh_size / shdr.sh_entsize;
    }
    else if(!strcmp(".data",name))
    {
      e->dataData=elf_getdata(scn,NULL);
      e->dataStart=shdr.sh_addr;
      //printf("data size is 0x%x at offset 0x%x\n",dataData->d_size,(uint)dataData->d_off);
      //printf("data section starts at address 0x%x\n",dataStart);
    }
    else if(!strcmp(".text",name))
    {
      e->textData=elf_getdata(scn,NULL);
      e->textStart=shdr.sh_addr;
    }
  }
}

