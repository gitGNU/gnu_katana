/*
  File: symbol.c
  Author: James Oakley
  Project: Katana
  Date: February 2010
  Description: Functions for dealing with symbols in ELF files
*/

#include "symbol.h"
#include <assert.h>

bool getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym)
{
  assert(e->symTabData);
  return gelf_getsym(e->symTabData,symIdx,outSym);
}

addr_t getSymAddress(ElfInfo* e,int symIdx)
{
  assert(e->symTabData);
  GElf_Sym sym;
  if(!gelf_getsym(e->symTabData,symIdx,&sym))
  {
    death("gelf_getsym failed in getSymAddress\n");
  }
  return sym.st_value;
}

//find the symbol matching the given symbol
int findSymbol(ElfInfo* e,GElf_Sym* sym,ElfInfo* ref)
{
  char* symbolName=getString(ref,sym->st_name);
  //traverse the symbol table to find the symbol we're looking for. Yes this is slow
  //todo: build our own hash table since the .hash section seems incomplete
  for (int i = 0; i < e->symTabCount; ++i)
  {
    Elf32_Sym sym2;
    //get the symbol in an unsave manner because
    //we may be getting it from a data buffer we're in the process of filling
    memcpy(&sym2,e->symTabData->d_buf+i*sizeof(Elf32_Sym),sizeof(Elf32_Sym));
    char* symname=getString(e, sym2.st_name);
    if((!symname && !symbolName) ||
       (!symname && !strlen(symbolName)) ||
       (!strlen(symname) && !symbolName) ||
       (symname && symbolName && !strcmp(symname,symbolName)))
    {
      if(symname && strlen(symname))
      {
        printf("[%i] cmatches name %s\n",i,symname);
      }
      else
      {
        //printf("[%i] both have no name. They might be the same section\n",i);
      }
      //ok, the right name, but are other things right too?
      int bind=ELF64_ST_BIND(sym->st_info);
      int type=ELF64_ST_TYPE(sym->st_info);
      int bind2=ELF32_ST_BIND(sym2.st_info);
      int type2=ELF32_ST_TYPE(sym2.st_info);
      if(bind != bind2)
      {
        printf("fails on bind\n");
        continue;
      }
      if(type != type2)
      {
        continue;
      }

      //don't match on size because the size of a variable may
      //have changed
      
      if(sym->st_other!=sym2.st_other)
      {
        printf("fails on other\n");
        continue;
      }
      //now the hard one to deal with: section index. This is especially
      //important to deal with for section symbols though as there may be no other
      //means of differentiating them
      int shndxRef=sym->st_shndx;
      int shndxNew=sym2.st_shndx;
      //allowing undefined to be a wildcard because may be bringing
      //in a symbol from a relocatable object
      if(shndxRef!=SHN_UNDEF && shndxNew!=SHN_UNDEF)
      {
        Elf_Scn* scnRef=elf_getscn(ref->e,shndxRef);
        Elf_Scn* scnNew=elf_getscn(e->e,shndxNew);
        GElf_Shdr shdrRef;
        GElf_Shdr shdrNew;
        gelf_getshdr(scnRef,&shdrRef);
        gelf_getshdr(scnNew,&shdrNew);
        char* scnNameRef=getScnHdrString(ref,shdrRef.sh_name);
        char* scnNameNew=getScnHdrString(e,shdrNew.sh_name);
        //printf("old refers to section name %s and new refers to section name %s\n",scnNameRef,scnNameNew);
        if((scnNameRef && !scnNameNew) || (!scnNameNew && scnNameNew) ||
           (scnNameRef && scnNameNew && strcmp(scnNameRef,scnNameNew)))
        {
          continue;
        }
      }
      return i;
    }
    //free(symname);//todo: is this right?
  }
  fprintf(stderr,"Symbol '%s' not defined\n",symbolName);
  return STN_UNDEF;
}

GElf_Sym symToGELFSym(Elf32_Sym sym)
{
  GElf_Sym res;
  res.st_name=sym.st_name;
  res.st_value=sym.st_value;
  res.st_size=sym.st_size;
  res.st_info=ELF64_ST_INFO(ELF32_ST_BIND(sym.st_info),
                            ELF32_ST_TYPE(sym.st_info));
  res.st_other=sym.st_other;
  res.st_shndx=sym.st_shndx;
  return res;
}

//from an index of a symbol in the old ELF structure,
//find it's index in the new ELF structure. Return -1 if it cannot be found
//The matching is done solely on name
int reindexSymbol(ElfInfo* old,ElfInfo* new,int oldIdx)
{
  //todo: need a method of apropriately handling local/week symbols
  //(other than section ones, which aren't really a big deal to handle)
  //we should be able to use the FILE symbols placed above local symbols
  //to do this
  
  GElf_Sym sym;
  //todo: in the future we could match on other things beside name
  //in case name was duplicated for some reason
  if(!getSymbol(old,oldIdx,&sym))
  {
    death("invalid symbol index in reindexSymbol\n");
  }
  int idx=findSymbol(new,&sym,old);
  return idx;
}

