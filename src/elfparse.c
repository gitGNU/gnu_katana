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
#include "util/logging.h"

ElfInfo* openELFFile(char* fname)
{
  ElfInfo* e=zmalloc(sizeof(ElfInfo));
  e->fname=strdup(fname);
  e->fd=open(fname,O_RDONLY);
  if(e->fd < 0)
  {
    char buf[128];
    snprintf(buf,128,"Failed to open elf file %s (open returned invalid fd)\n",fname);
    death(buf);
  }
  e->e=elf_begin(e->fd,ELF_C_READ,NULL);
  if(!e->e)
  {
    fprintf(stderr,"Failed to open as an ELF file %s\n",elf_errmsg(-1));
    free(e);
    death(NULL);
  }
  findELFSections(e);
  return e;
}

void endELF(ElfInfo* e)
{
  elf_end(e->e);
  close(e->fd);
  free(e);
}


void* getDataAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type)
{
  assert(scn);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  Elf_Data* data=elf_getdata(scn,NULL);
  assert(data->d_size >= offset);
  return data->d_buf+offset;
}

void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  return textData->d_buf+offset;
  //todo: merge this func with getDataAtAbs?
}

word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  return *((word_t*)(textData->d_buf+offset));
  //todo: merge this func with getWordAtAbs?
}

word_t getWordAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type)
{
  assert(scn);
  Elf_Data* data=elf_getdata(scn,NULL);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  assert(data->d_size >= offset);//todo: shouldn't it actually be offset plus size of word?
                                 //doing that was causing oddness though
  return *((word_t*)(data->d_buf+offset));
}

void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  uint offset=addr-e->textStart[type];
  assert(textData->d_size >= offset);
  memcpy(textData->d_buf+offset,&value,sizeof(word_t));
}

void setWordAtAbs(Elf_Scn* scn,addr_t addr,word_t value,ELF_STORAGE_TYPE type)
{
  assert(scn);
  Elf_Data* data=elf_getdata(scn,NULL);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {
    death("cannot get shdr for scn\n");
  }
  uint offset=addr-(IN_MEM==type?shdr.sh_addr:shdr.sh_offset);
  assert(data->d_size >= offset+sizeof(word_t));
  memcpy(data->d_buf+offset,&value,sizeof(word_t));
}

void* getTextDataAtRelOffset(ElfInfo* e,int offset)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  return textData->d_buf+offset;
}

word_t getTextAtRelOffset(ElfInfo* e,int offset)
{
  Elf_Data* textData=getDataByERS(e,ERS_TEXT);
  return *((word_t*)(textData->d_buf+offset));
}


int getSymtabIdx(ElfInfo* e,char* symbolName)
{
  //todo: need to consider endianness?
  //assert(e->hashTableData);
  assert(e->strTblIdx>0);
  
  //Hash table only contains dynamic symbols, don't use it here
  /*
  int nbucket, nchain;
  memcpy(&nbucket,hashTableData->d_buf,sizeof(Elf32_Word));
  memcpy(&nchain,hashTableData->d_buf+sizeof(Elf32_Word),sizeof(Elf32_Word));
  logprintf(ELL_INFO_V4,ELS_MISC,"there are %i buckets, %i chain entries, and total size of %i\n",nbucket,nchain,hashTableData->d_size);


  unsigned long int hash=elf_hash(symbolName)%nbucket;
  logprintf(ELL_INFO_V4,ELS_MISC,"hash is %lu\n",hash);
  //get the index from the bucket
  int idx;
  memcpy(&idx,hashTableData->d_buf+sizeof(Elf32_Word)*(2+hash),sizeof(Elf32_Word));
  int nextIdx=idx;
  char* symname="";
  while(strcmp(symname,symbolName))
  {
    idx=nextIdx;
    logprintf(ELL_INFO_V4,ELS_MISC,"idx is %i\n",idx);
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
    Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
    gelf_getsym(symTabData,i,&sym);
    char* symname=elf_strptr(e->e, e->strTblIdx, sym.st_name);
    if(!strcmp(symname,symbolName))
    {
      return i;
    }
    //free(symname);//todo: is this right?
  }
  fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
  return STN_UNDEF;
}

void printSymTab(ElfInfo* e)
{
  logprintf(ELL_INFO_V4,ELS_MISC,"symbol table:\n");
  /* print the symbol names */
  for (int i = 0; i < e->symTabCount; ++i)
  {
    GElf_Sym sym;
    Elf_Data* symTabData=getDataByERS(e,ERS_SYMTAB);
    gelf_getsym(symTabData, i, &sym);
    logprintf(ELL_INFO_V4,ELS_MISC,"%i. %s\n", i,elf_strptr(e->e, e->strTblIdx, sym.st_name));
  }

}

//have to pass the name that the elf file will originally get written
//out to, because of the way elf_begin is set up
ElfInfo* duplicateElf(ElfInfo* e,char* outfname,bool flushToDisk,bool keepLayout)
{
  int outfd = creat(outfname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", outfname);
  }
  //code inspired by ecp in elfutils tests
  Elf *outelf = elf_begin(outfd, ELF_C_WRITE, NULL);

  gelf_newehdr (outelf, gelf_getclass(e->e));
  GElf_Ehdr ehdr;
  if(!gelf_getehdr(e->e,&ehdr))
  {
    fprintf(stdout,"Failed to get ehdr from elf file we're duplicating: %s\n",elf_errmsg (-1));
    death(NULL);
  }
  gelf_update_ehdr(outelf,&ehdr);
  if(ehdr.e_phnum > 0)
  {
    int cnt;
    gelf_newphdr(outelf,ehdr.e_phnum);
    for (cnt = 0; cnt < ehdr.e_phnum; ++cnt)
    {
      GElf_Phdr phdr_mem;
      if(!gelf_getphdr(e->e,cnt,&phdr_mem))
      {
        fprintf(stdout,"Failed to get phdr from elf file we're duplicating: %s\n",elf_errmsg (-1));
        death(NULL);
      }
      gelf_update_phdr(outelf,cnt,&phdr_mem);
    }
  }
  Elf_Scn* scn=NULL;
  while ((scn = elf_nextscn(e->e,scn)))
  {
    Elf_Scn *newscn = elf_newscn (outelf);
    GElf_Shdr shdr;
    gelf_update_shdr(newscn,gelf_getshdr(scn,&shdr));
    Elf_Data* newdata=elf_newdata(newscn);
    Elf_Data* data=elf_getdata (scn,NULL);
    assert(data);
    newdata->d_off=data->d_off;
    newdata->d_size=data->d_size;
    newdata->d_align=data->d_align;
    newdata->d_version=data->d_version;
    newdata->d_type=data->d_type;
    if(SHT_NOBITS!=shdr.sh_type)
    {
      newdata->d_buf=zmalloc(newdata->d_size);
      //printf("size is %i in scn idx %i\n",data->d_size,elf_ndxscn(scn));
      memcpy(newdata->d_buf,data->d_buf,data->d_size);
    }
  }


  elf_flagelf(outelf, ELF_C_SET, ELF_F_DIRTY);
  if(keepLayout)
  {
      elf_flagelf(outelf,ELF_C_SET,ELF_F_LAYOUT);
  }

  if (elf_update(outelf, flushToDisk?ELF_C_WRITE:ELF_C_NULL) <0)
  {
    fprintf(stdout,"Failed to write out elf file: %s\n",elf_errmsg (-1));
  }
  ElfInfo* newE=zmalloc(sizeof(ElfInfo));
  newE->fd=outfd;
  newE->fname=outfname;
  newE->e=outelf;
  findELFSections(newE);
  return newE;
}

void writeOut(ElfInfo* e,char* outfname,bool keepLayout)
{
  ElfInfo* newE=duplicateElf(e,outfname,true,keepLayout);
  close(newE->fd);
  elf_end(newE->e);
  free(newE);
}

Elf_Data* getDataByIdx(ElfInfo* e,idx_t idx)
{
  Elf_Scn* scn=elf_getscn(e->e,idx);
  assert(scn);
  return elf_getdata(scn,NULL);
}

Elf_Data* getDataByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  return elf_getdata(scn,NULL);
}

Elf_Scn* getSectionByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  return scn;
}

void getShdrByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers,GElf_Shdr* shdr)
{
  assert(e->sectionIndices[ers]);
  Elf_Scn* scn=elf_getscn(e->e,e->sectionIndices[ers]);
  assert(scn);
  if(!gelf_getshdr(scn,shdr))
  {
    death("Could not get shdr for section in getShdrByERS\n");
  }
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
      logprintf(ELL_INFO_V4,ELS_MISC,"found symbol hash table\n");
      e->sectionIndices[ERS_HASHTABLE]=elf_ndxscn(scn);
    }
    else if(!strcmp(".symtab",name))
    {
      e->sectionIndices[ERS_SYMTAB]=elf_ndxscn(scn);
      e->symTabCount = shdr.sh_size / shdr.sh_entsize;
      e->strTblIdx=shdr.sh_link;
    }
    else if(!strcmp(".strtab",name))
    {
      e->sectionIndices[ERS_STRTAB]=elf_ndxscn(scn);
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
      e->sectionIndices[ERS_DATA]=elf_ndxscn(scn);
      e->dataStart[IN_MEM]=shdr.sh_addr;
      e->dataStart[ON_DISK]=shdr.sh_offset;
      //printf("data size is 0x%x at offset 0x%x\n",dataData->d_size,(uint)dataData->d_off);
      //printf("data section starts at address 0x%x\n",dataStart);
    }
    else if(!strncmp(".text",name,strlen(".text"))) //allow versioned text sections in patches as well as .text
    {
      e->sectionIndices[ERS_TEXT]=elf_ndxscn(scn);
      e->textStart[IN_MEM]=shdr.sh_addr;
      e->textStart[ON_DISK]=shdr.sh_offset;
    }
    else if(!strncmp(".rodata",name,strlen(".rodata"))) //allow versioned
                         //sections in patches
    {
      e->sectionIndices[ERS_RODATA]=elf_ndxscn(scn);
    }
    else if(!strncmp(".rela.text",name,strlen(".rela.text"))) //allow versioned
                         //sections in patches
    {
      e->sectionIndices[ERS_RELA_TEXT]=elf_ndxscn(scn);
    }
    else if(!strcmp(".plt",name))
    {
      e->sectionIndices[ERS_PLT]=elf_ndxscn(scn);
    }
    else if(!strcmp(".rel.plt",name))
    {
      e->sectionIndices[ERS_REL_PLT]=elf_ndxscn(scn);
    }
    else if(!strcmp(".dynsym",name))
    {
      e->sectionIndices[ERS_DYNSYM]=elf_ndxscn(scn);
    }
    else if(!strcmp(".dynstr",name))
    {
      e->sectionIndices[ERS_DYNSTR]=elf_ndxscn(scn);
    }
  }
}

Elf_Scn* getSectionByName(ElfInfo* e,char* name)
{
  assert(e->sectionHdrStrTblIdx);
  for(Elf_Scn* scn=elf_nextscn (e->e,NULL);scn;scn=elf_nextscn(e->e,scn))
  {
    GElf_Shdr shdr;
    if(!gelf_getshdr(scn,&shdr))
    {
      death("cannot get shdr\n");
    }
    char* scnName=elf_strptr(e->e,e->sectionHdrStrTblIdx,shdr.sh_name);
    if(!strcmp(name,scnName))
    {
      return scn;
    }
  }
  return NULL;
}

char* getSectionNameFromIdx(ElfInfo* e,int idx)
{
  Elf_Scn* scn=elf_getscn(e->e,idx);
  if(!scn)
  {
    return NULL;
  }
  GElf_Shdr shdr;
  gelf_getshdr(scn,&shdr);
  return elf_strptr(e->e,e->sectionHdrStrTblIdx,shdr.sh_name);
}


char* getString(ElfInfo* e,int idx)
{
  Elf_Scn* scn=elf_getscn(e->e,e->strTblIdx);
  Elf_Data* data=elf_getdata(scn,NULL);
  return (char*)data->d_buf+idx;
}

char* getDynString(ElfInfo* e,int idx)
{
  Elf_Scn* scn=getSectionByERS(e,ERS_DYNSTR);
  Elf_Data* data=elf_getdata(scn,NULL);
  return (char*)data->d_buf+idx;
}

char* getScnHdrString(ElfInfo* e,int idx)
{
  Elf_Scn* scn=elf_getscn(e->e,e->sectionHdrStrTblIdx);
  Elf_Data* data=elf_getdata(scn,NULL);
  return (char*)data->d_buf+idx;
}
