/*
  File: elfwriter.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: February 2010
  Description: routines for building an elf file for a patch-> Not thread safe
*/

#include "elfwriter.h"
#include <assert.h>
#include <fcntl.h>
#include <util/logging.h>

//Elf_Data* patch_syms_rel_data=NULL;
//Elf_Data* patch_syms_new_data=NULL;
Elf_Data* patch_rules_data=NULL;
Elf_Data* patch_expr_data=NULL;
Elf_Data* strtab_data=NULL;
Elf_Data* symtab_data=NULL;
Elf_Data* text_data=NULL;//for storing new versions of functions
Elf_Data* rodata_data=NULL;
Elf_Data* rela_text_data=NULL;
//Elf_Data* old_symtab_data=NULL;

//Elf_Scn* patch_syms_rel_scn=NULL;
//Elf_Scn* patch_syms_new_scn=NULL;

Elf_Scn* patch_rules_scn=NULL;
Elf_Scn* patch_expr_scn=NULL;
Elf_Scn* strtab_scn=NULL;
Elf_Scn* symtab_scn=NULL;
Elf_Scn* text_scn=NULL;//for storing new versions of functions
Elf_Scn* rodata_scn=NULL;//hold the new rodata from the patch
Elf_Scn* rela_text_scn=NULL;
//now not writing old symbols b/c
//better to get the from /proc/PID/exe
/*Elf_Scn* old_symtab_scn=NULL;//relevant symbols from the symbol table of the old binary
                             //we must store this in the patch object in case the object
                             //in memory does not have a symbol table loaded in memory
                             */

//todo: use smth like this in the future rather than having a separate variable for everything
//currently this isn't used for everything
typedef struct
{
  Elf_Scn* scn;
  Elf_Data* data;
  //size_t allocatedSize;//bytes allocated for data->d_buf
} ScnInProgress;

ScnInProgress scnInfo[ERS_CNT];


ElfInfo* patch=NULL;
Elf* outelf;

//returns the offset into the section that the data was added at
addr_t addDataToScn(Elf_Data* dataDest,void* data,int size)
{
  dataDest->d_buf=realloc(dataDest->d_buf,dataDest->d_size+size);
  MALLOC_CHECK(dataDest->d_buf);
  memcpy((byte*)dataDest->d_buf+dataDest->d_size,data,size);
  dataDest->d_size=dataDest->d_size+size;
  return dataDest->d_size-size;
}

//adds an entry to the string table, return its offset
int addStrtabEntry(char* str)
{
  int len=strlen(str)+1;
  addDataToScn(strtab_data,str,len);
  elf_flagdata(strtab_data,ELF_C_SET,ELF_F_DIRTY);
  return strtab_data->d_size-len;
}

//return index of entry in symbol table
int addSymtabEntry(Elf_Data* data,ElfXX_Sym* sym)
{
  int len=sizeof(ElfXX_Sym);
  addDataToScn(data,sym,len);
  if(data==symtab_data)
  {
    patch->symTabCount=data->d_off/len;
  }
  return (data->d_size-len)/len;
}

void createSections(Elf* outelf)
{
  patch->dataAllocatedByKatana=true;
  //first create the string table
  strtab_scn=elf_newscn(outelf);
  strtab_data=elf_newdata(strtab_scn);
  strtab_data->d_align=1;
  strtab_data->d_buf=NULL;
  strtab_data->d_off=0;
  strtab_data->d_size=0;
  strtab_data->d_version=EV_CURRENT;
  scnInfo[ERS_STRTAB].scn=strtab_scn;
  scnInfo[ERS_STRTAB].data=strtab_data;
  
  ElfXX_Shdr* shdr;
  shdr=elfxx_getshdr(strtab_scn);
  shdr->sh_type=SHT_STRTAB;
  shdr->sh_link=SHN_UNDEF;
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=1;//first real entry in the string table

  
  
  addStrtabEntry("");//first entry in stringtab null so can have normal unnamed null section
                     //todo: what is the purpose of this?
  addStrtabEntry(".strtab");

  //create the data section
  //we use this for holding initializers for new variables or for new fields
  //for existing variables. At present, for simplicity's sake, we have a full
  //entry in here for data objects we are going to transform although in the future
  //it might possible make sense to have a bss section for them so they don't take
  //up too much space in the patch
  Elf_Scn* scn=scnInfo[ERS_DATA].scn=elf_newscn(outelf);

  Elf_Data* data=scnInfo[ERS_DATA].data=elf_newdata(scn);
  data->d_align=1;
  data->d_version=EV_CURRENT;
  shdr=elfxx_getshdr(scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=SHN_UNDEF;
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;  //todo: should this be word-aligned? It seems
                         //that it is in the ELF files I've examined,
                         //but does it have to be?
  shdr->sh_flags=SHF_ALLOC | SHF_WRITE;
  shdr->sh_name=addStrtabEntry(".data.new");



  #ifdef RENAMED_DWARF_SCNS
  
  //now create patch rules
  patch_rules_scn=elf_newscn(outelf);
  patch_rules_data=elf_newdata(patch_rules_scn);
  patch_rules_data->d_align=1;
  patch_rules_data->d_buf=NULL;
  patch_rules_data->d_off=0;
  patch_rules_data->d_size=0
  patch_rules_data->d_version=EV_CURRENT;
  
  shdr=elfxx_getshdr(patch_rules_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_rules");

  //now create patch expressions
  patch_expr_scn=elf_newscn(outelf);
  patch_expr_data=elf_newdata(patch_expr_scn);
  patch_expr_data->d_align=1;
  patch_expr_data->d_buf=NULL;
  patch_expr_data->d_off=0;
  patch_expr_data->d_size=0;
  patch_expr_data->d_version=EV_CURRENT;
  
  shdr=elfxx_getshdr(patch_expr_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=addStrtabEntry(".patch_expr");

  #endif

  //ordinary symtab
  symtab_scn=elf_newscn(outelf);
  symtab_data=elf_newdata(symtab_scn);
  symtab_data->d_align=1;
  symtab_data->d_buf=NULL;
  symtab_data->d_off=0;
  symtab_data->d_size=0;
  symtab_data->d_version=EV_CURRENT;
  scnInfo[ERS_SYMTAB].scn=symtab_scn;
  scnInfo[ERS_SYMTAB].data=symtab_data;
  
  shdr=elfxx_getshdr(symtab_scn);
  shdr->sh_type=SHT_SYMTAB;
  shdr->sh_link=1;//index of string table
  shdr->sh_info=0;//todo: p.1-13 of ELF format describes this,
                          //but I don't quite understand it
  shdr->sh_addralign=__WORDSIZE;
  shdr->sh_entsize=sizeof(ElfXX_Sym);
  shdr->sh_name=addStrtabEntry(".symtab");

  //first symbol in symtab should be all zeros
  ElfXX_Sym sym;
  memset(&sym,0,sizeof(ElfXX_Sym));
  addSymtabEntry(symtab_data,&sym);

   //create the section for holding indices to symbols of unsafe
  //functions that can't have activation frames during patching
  scn=scnInfo[ERS_UNSAFE_FUNCTIONS].scn=elf_newscn(outelf);

  data=scnInfo[ERS_UNSAFE_FUNCTIONS].data=elf_newdata(scn);
  data->d_align=sizeof(idx_t);
  data->d_version=EV_CURRENT;
  shdr=elfxx_getshdr(scn);
  shdr->sh_type=SHT_KATANA_UNSAFE_FUNCTIONS;
  shdr->sh_link=elf_ndxscn(symtab_scn);
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;  //todo: should this be word-aligned? It seems
                         //that it is in the ELF files I've examined,
                         //but does it have to be?
  shdr->sh_name=addStrtabEntry(".unsafe_functions");

  //text section for new functions
  text_scn=elf_newscn(outelf);
  text_data=elf_newdata(text_scn);
  text_data->d_align=1;
  text_data->d_buf=NULL;
                                       //will be allocced as needed
  text_data->d_off=0;
  text_data->d_size=0;
  text_data->d_version=EV_CURRENT;
  
  shdr=elfxx_getshdr(text_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=0;
  shdr->sh_info=0;
  shdr->sh_addralign=1;//normally text is aligned, but we never actually execute from this section
  shdr->sh_name=addStrtabEntry(".text.new");
  shdr->sh_addr=0;//going to have to relocate anyway so no point in trying to keep the same address
  shdr->sh_flags=SHF_ALLOC | SHF_EXECINSTR;
  scnInfo[ERS_TEXT].scn=text_scn;
  scnInfo[ERS_TEXT].data=text_data;

  //rodata section for new strings, constants, etc
  //(note that in many cases these may not actually be "new" ones,
  //but unfortunately because .rodata is so unstructured, it can
  //be difficult to determine what is needed and what is not
  rodata_scn=elf_newscn(outelf);
  rodata_data=elf_newdata(rodata_scn);
  rodata_data->d_align=1;
  rodata_data->d_buf=NULL;
  rodata_data->d_off=0;
  rodata_data->d_size=0;
  rodata_data->d_version=EV_CURRENT;
  scnInfo[ERS_RODATA].scn=rodata_scn;
  scnInfo[ERS_RODATA].data=rodata_data;
  
  shdr=elfxx_getshdr(rodata_scn);
  shdr->sh_type=SHT_PROGBITS;
  shdr->sh_link=0;
  shdr->sh_info=0;
  shdr->sh_addralign=1;//normally text is aligned, but we never actually execute from this section
  shdr->sh_flags=SHF_ALLOC;
  shdr->sh_name=addStrtabEntry(".rodata.new");

  //rela.text.new
  rela_text_scn=elf_newscn(outelf);
  rela_text_data=elf_newdata(rela_text_scn);
  rela_text_data->d_align=1;
  rela_text_data->d_buf=NULL;
  rela_text_data->d_off=0;
  rela_text_data->d_size=0;
  rela_text_data->d_version=EV_CURRENT;
  scnInfo[ERS_RELA_TEXT].scn=rela_text_scn;
  scnInfo[ERS_RELA_TEXT].data=rela_text_data;
  shdr=elfxx_getshdr(rela_text_scn);
  shdr->sh_type=SHT_RELA;
  shdr->sh_addralign=__WORDSIZE;
  shdr->sh_name=addStrtabEntry(".rela.text.new");

  //write symbols for sections
  sym.st_info=ELFXX_ST_INFO(STB_LOCAL,STT_SECTION);
  for(int i=0;i<ERS_CNT;i++)
  {
    if(scnInfo[i].scn)
    {
      sym.st_name=elfxx_getshdr(scnInfo[i].scn)->sh_name;
      sym.st_shndx=elf_ndxscn(scnInfo[i].scn);
      addSymtabEntry(symtab_data,&sym);
    }
  }


  //fill in some info about the sections we added
  patch->sectionIndices[ERS_TEXT]=elf_ndxscn(text_scn);
  patch->sectionIndices[ERS_SYMTAB]=elf_ndxscn(symtab_scn);
  patch->sectionIndices[ERS_STRTAB]=elf_ndxscn(strtab_scn);
  patch->sectionIndices[ERS_RODATA]=elf_ndxscn(rodata_scn);
  patch->sectionIndices[ERS_RELA_TEXT]=elf_ndxscn(rela_text_scn);
  patch->sectionIndices[ERS_DATA]=elf_ndxscn(scnInfo[ERS_DATA].scn);
  patch->sectionIndices[ERS_UNSAFE_FUNCTIONS]=elf_ndxscn(scnInfo[ERS_UNSAFE_FUNCTIONS].scn);
}

//must be called before any other routines
//for each patch object to create
ElfInfo* startPatchElf(char* fname)
{
  patch=zmalloc(sizeof(ElfInfo));
  patch->isPO=true;
  int outfd = creat(fname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", fname);
  }

  outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
  patch->e=outelf;
  ElfXX_Ehdr* ehdr=elfxx_newehdr(outelf);
  if(!ehdr)
  {
    death("Unable to create new ehdr\n");
  }
  ehdr->e_ident[EI_MAG0]=ELFMAG0;
  ehdr->e_ident[EI_MAG1]=ELFMAG1;
  ehdr->e_ident[EI_MAG2]=ELFMAG2;
  ehdr->e_ident[EI_MAG3]=ELFMAG3;
  ehdr->e_ident[EI_CLASS]=ELFCLASSXX;
  ehdr->e_ident[EI_DATA]=ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION]=EV_CURRENT;
  ehdr->e_ident[EI_OSABI]=ELFOSABI_LINUX;//todo: support systems other than Linux
#ifdef KATANA_X86_ARCH
  ehdr->e_machine=EM_386;
#elif defined(KATANA_X86_64_ARCH)
  ehdr->e_machine=EM_X86_64;
#else
#error Unknown architecture
#endif
  
  ehdr->e_type=ET_NONE;//not relocatable, or executable, or shared object, or core, etc
  ehdr->e_version=EV_CURRENT;

  createSections(outelf);
  ehdr->e_shstrndx=elf_ndxscn(strtab_scn);//set strtab in elf header
  //todo: perhaps the two string tables should be separate
  patch->strTblIdx=patch->sectionHdrStrTblIdx=ehdr->e_shstrndx;
  return patch;
}



void finalizeDataSize(Elf_Scn* scn,Elf_Data* data)
{
  ElfXX_Shdr* shdr=elfxx_getshdr(scn);
  shdr->sh_size=data->d_size;
  logprintf(ELL_INFO_V3,ELS_ELFWRITE,"finalizing data size to %i for section with index %i\n",shdr->sh_size,elf_ndxscn(scn));
}

void finalizeDataSizes()
{
  finalizeDataSize(strtab_scn,strtab_data);
  #ifdef RENAMED_DWARF_SCNS
  
  //then patch expressions
  finalizeDataSize(patch_expr_scn,patch_expr_data);
  #endif

  //ordinary symtab
  finalizeDataSize(symtab_scn,symtab_data);
  //now not using old symtab b/c better to get old symbols from
  // /proc/PID/exe
  /*
  //symtab from old binary

  shdr=elfxx_getshdr(old_symtab_scn);
  shdr->sh_size=old_symtab_data->d_size;
  */

  finalizeDataSize(text_scn,text_data);
  finalizeDataSize(rela_text_scn,rela_text_data);
  finalizeDataSize(rodata_scn,rodata_data);
}

void endPatchElf()
{

  finalizeDataSizes();
  
      //don't actually have to reindex symbols because everything is set up with a zero address

  /*
  if(elf_update (outelf, ELF_C_NULL) <0)
  {
    death("Failed to write out elf file: %s\n",elf_errmsg (-1));
    exit(1);
  }


  //all symbols created so far that were relative to sections
  //assumed that their sections started at location 0. This obviously
  //can't be true of all sections. We now relocate the symbols appropriately
  int numEntries=symtab_data->d_size/sizeof(ElfXX_Sym);
  for(int i=1;i<numEntries;i++)
  {
    ElfXX_Sym* sym=symtab_data->d_buf+i*sizeof(ElfXX_Sym);
    if(sym->st_shndx!=SHN_UNDEF && sym->st_shndx!=SHN_ABS && sym->st_shndx!=SHN_COMMON)
    {
      //symbol needs rebasing
      Elf_Scn* scn=elf_getscn(outelf,sym->st_shndx);
      assert(scn);
      GElf_Shdr shdr;
      if(!gelf_getshdr(scn,&shdr))
      {death("gelf_getshdr failed\n");}
      sym->st_value+=shdr.sh_addr;
    }
    }*/

  if(elf_update (outelf, ELF_C_WRITE) <0)
  {
    death("Failed to write out elf file: %s\n",elf_errmsg (-1));
    exit(1);
  }  
  
  endELF(patch);
  patch_rules_data=patch_expr_data=strtab_data=text_data=rodata_data=rela_text_data=NULL;
  patch_rules_scn=patch_expr_scn=strtab_scn=text_scn=rodata_scn=rela_text_scn=NULL;
}

int reindexSectionForPatch(ElfInfo* e,int scnIdx)
{
  char* scnName=getSectionNameFromIdx(e,scnIdx);
  if(!strcmp(".rodata",scnName))
  {
    return elf_ndxscn(rodata_scn);
  }
  else
  {
    //perhaps we have a section with that name
    Elf_Scn* patchScn=getSectionByName(patch,scnName);
    if(!patchScn)
    {
      //ok, we have to create a section with that name
      //just so we can refer to it with a symbol
      //so that it can get reindexed to the appropriate section
      //in the original binary when we apply the patch
      Elf_Scn* scn=elf_getscn(e->e,scnIdx);
      GElf_Shdr shdr;
      getShdr(scn,&shdr);
      patchScn=elf_newscn(outelf);
      ElfXX_Shdr* patchShdr=elfxx_getshdr(patchScn);
      patchShdr->sh_type=shdr.sh_type;
      patchShdr->sh_link=shdr.sh_link;
      patchShdr->sh_info=shdr.sh_info;
      patchShdr->sh_addralign=shdr.sh_addralign;
      patchShdr->sh_name=addStrtabEntry(scnName);
      //need to add a symbol for this section
      ElfXX_Sym sym;
      memset(&sym,0,sizeof(ElfXX_Sym));
      sym.st_info=ELFXX_ST_INFO(STB_LOCAL,STT_SECTION);
      sym.st_name=patchShdr->sh_name;
      sym.st_shndx=elf_ndxscn(patchScn);
      addSymtabEntry(symtab_data,&sym);
      
    }
    return elf_ndxscn(patchScn);
  }
  return STN_UNDEF;
}

int dwarfWriteSectionCallback(char* name,int size,Dwarf_Unsigned type,
                              Dwarf_Unsigned flags,Dwarf_Unsigned link,
                              Dwarf_Unsigned info,int* sectNameIdx,int* error)
{
  //look through all the sections for one which matches to
  //see if we've already created this section
  Elf_Scn* scn=elf_nextscn(outelf,NULL);
  int nameLen=strlen(name);
  Elf_Data* symtab_data=getDataByERS(patch,ERS_SYMTAB);
  Elf_Data* strtab_data=getDataByERS(patch,ERS_STRTAB);
  for(;scn;scn=elf_nextscn(outelf,scn))
  {
    ElfXX_Shdr* shdr=elfxx_getshdr(scn);
    if(!strncmp(strtab_data->d_buf+shdr->sh_name,name,nameLen))
    {
      //ok, we found the section we want, now have to find its symbol
      int symtabSize=symtab_data->d_size;
      for(int i=0;i<symtabSize/sizeof(ElfXX_Sym);i++)
      {
        ElfXX_Sym* sym=(ElfXX_Sym*)(symtab_data->d_buf+i*sizeof(ElfXX_Sym));
        int idx=elf_ndxscn(scn);
        //printf("we're on section with index %i and symbol for index %i\n",idx,sym->st_shndx);
        if(STT_SECTION==ELFXX_ST_TYPE(sym->st_info) && idx==sym->st_shndx)
        {
          *sectNameIdx=i;
          return idx;
        }
      }
      fprintf(stderr,"finding existing section for %s\n",name);
      death("found section already existing but had no symbol, this should be impossible\n");
    }
  }

  //section doesn't already exist, create it
 
  //todo: write this section
  scn=elf_newscn(outelf);
  ElfXX_Shdr* shdr=elfxx_getshdr(scn);
  shdr->sh_name=addStrtabEntry(name);
  shdr->sh_type=type;
  shdr->sh_flags=flags;
  shdr->sh_size=size;
  shdr->sh_link=link;
  if(0==link && SHT_REL==type)
  {
    //make symtab the link
    shdr->sh_link=elf_ndxscn(symtab_scn);
  }
  shdr->sh_info=info;
  ElfXX_Sym sym;
  sym.st_name=shdr->sh_name;
  sym.st_value=0;//don't yet know where this symbol will end up. todo: fix this, so relocations can theoretically be done
  sym.st_size=0;
  sym.st_info=ELFXX_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_other=0;
  sym.st_shndx=elf_ndxscn(scn);
  *sectNameIdx=addSymtabEntry(symtab_data,&sym);
  *error=0;  
  return sym.st_shndx;
}
