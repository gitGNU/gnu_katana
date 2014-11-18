/*
  File: patchapply.c
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
  Date: January 2010
  Description: Read patch object files and apply them
*/

#include "elfutil.h"
#include "dwarftypes.h"
#include "hotpatch.h"
#include "target.h"
#include "fderead.h"
#include "dwarfvm.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "relocation.h"
#include "versioning.h"
#include <assert.h>
#include "symbol.h"
#include "util/logging.h"
#include "linkmap.h"
#include "safety.h"
#include "info/fdedump.h"
#include "constants.h"
#include <sys/wait.h>
#include <unistd.h>
#include "pmap.h"
#include "patchapply.h"
#include "katana_config.h"

ElfInfo* patchedBin=NULL;
ElfInfo* targetBin=NULL;
addr_t patchTextAddr=0;
addr_t patchRodataAddr=0;
addr_t patchRelTextAddr=0;
addr_t patchDataAddr=0;

int addStrtabEntryToExisting(ElfInfo* e,char* str,bool header);
int addSymtabEntryToExisting(ElfInfo* e,ElfXX_Sym* sym);

void allocateMemoryForVarRelocation(VarInfo* var)
{
  printf("allocating memory for relocating variabl %s\n",var->name);
  int length=var->type->length;
  var->newLocation=getFreeSpaceInTarget(length);
  byte* zeros=zmalloc(length);
  printf("zeroing out new memory at 0x%lx with length %i\n",(unsigned long)var->newLocation,length);
  memcpyToTarget(var->newLocation,zeros,length);
  free(zeros);
  //todo: handle errors
}


void transformVarData(VarInfo* var,Map* fdeMap,ElfInfo* patch)
{
  logprintf(ELL_INFO_V2,ELS_PATCHAPPLY,"transforming var %s\n",var->name);
  FDE* transformerFDE=mapGet(fdeMap,&var->type->fde);
  if(!transformerFDE)
  {
    //todo: roll back atomically
    death("could not find transformer for variable %s referencing fde%i\n",var->name,var->type->fde);
  }
  patchDataWithFDE(var,transformerFDE,targetBin,patch,patchedBin);
}

void relocateVar(VarInfo* var,ElfInfo* targetBin)
{
  int symIdx=getSymtabIdx(targetBin,var->name,0);

  //do this first so that addend computation will be done
  //before we change the symtab entry
  List* relocItems=getRelocationItemsFor(patchedBin,symIdx);

  //record in the patched binary that we're putting the variable here
  GElf_Sym sym;
  getSymbol(targetBin,symIdx,&sym);
  sym.st_value=var->newLocation;
  //now we write the symbol to the new binary
  Elf_Data* symTabData=getDataByERS(patchedBin,ERS_SYMTAB);
  gelf_update_sym(symTabData,symIdx,&sym);

  logprintf(ELL_INFO_V4,ELS_PATCHAPPLY,"var new location is 0x%x\n",var->newLocation);

  //we do need to do this because may contain some relocations
  //not in new code
  applyRelocations(relocItems,IN_MEM);
}

void insertTrampolineJump(addr_t insertAt,addr_t jumpTo)
{
  printf("inserting at 0x%zx, address to jump to is 0x%zx\n",insertAt,jumpTo);
  
  //remember that the JMP absolute is indirect, have to specify
  //memory location which hold the memory location to jump to
#ifdef KATANA_X86_ARCH
  int len=2+sizeof(addr_t)*2;
  byte *code=zmalloc(len);
  code[0]=0xFF;//jmp instruction for a near absolute jump
  code[1]=0x25;//specify the addressing mode
  addr_t addrAddr=insertAt+2+sizeof(addr_t);//address of mem location holding jmp target
  memcpy(code+2,&addrAddr,sizeof(addr_t));
  memcpy(code+2+sizeof(addr_t),&jumpTo,sizeof(addr_t));
  #elif defined(KATANA_X86_64_ARCH)
  int len=2+4+sizeof(addr_t);
  byte *code=zmalloc(len);
  code[0]=0xFF;//jmp instruction for a near absolute jump
  code[1]=0x25;//specify the addressing mode
  addr_t addrAddr=0;//insertAt+2+4;//address of mem location holding jmp target
  memcpy(code+2,&addrAddr,sizeof(addr_t));
  memcpy(code+2+4,&jumpTo,sizeof(addr_t));
  #else
  #error Unknown architecture
  #endif
  
  memcpyToTarget(insertAt,code,len);
  //todo: probably don't need verify as memcpyToTarget can
  //be made to verify it's writings
  byte* verify=zmalloc(len);
  memcpyFromTarget(verify,insertAt,len);
  if(memcmp(code,verify,len))
  {
    death("failed to copy code into target properly\n");
  }
  free(code);
  free(verify);
}

void applyVariablePatch(VarInfo* var,Map* fdeMap,ElfInfo* patch)
{
  int idx=getSymtabIdx(targetBin,var->name,0);
  int symIdxInPatch=getSymtabIdx(patch,var->name,0);
  GElf_Sym symInPatch;
  if(STN_UNDEF!=symIdxInPatch)
  {
    getSymbol(patch,symIdxInPatch,&symInPatch);
    var->newLocation=patchDataAddr+symInPatch.st_value;
  }
  if(idx!=STN_UNDEF)
  {
    logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"Fixing up existing variable %s\n",var->name);
    GElf_Sym sym;
    //todo: should make space for the variable in .data.new or something
    getSymbol(targetBin,idx,&sym);
    var->oldLocation=sym.st_value;
    //now always relocating because the contents of .data.new may matter. todo: is this correct
    //bool relocate=sym.st_size < var->type->length;
    //printf("need to relocated is %i as st_size is %i and new size is %i\n",relocate,(int)sym.st_size,var->type->length);
    if(!var->newLocation)
    {
      var->newLocation=var->oldLocation;//didn't need to relocate the bar
    }
    if(var->type->fde)//might not have an fde if just a constant with a changed initializer
    {
      transformVarData(var,fdeMap,patch);
    }
    if(var->newLocation!=var->oldLocation)
    {
      //do need to do this because may contain some relocations
      //not in .rela.text.new
      relocateVar(var,targetBin);
    }
  }
  else
  {
    logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"Creating new variable %s\n",var->name);
    
    if(STN_UNDEF==symIdxInPatch)
    {
      death("patch symbol table didn't have symbol for new variable %s\n",var->name);
    }
    //create a symbol for our variable
    //todo: really should abstract this out
    ElfXX_Sym sym;
    memset(&sym,0,sizeof(ElfXX_Sym));
    //todo: might not always be the case that it's global
    sym.st_info=ELFXX_ST_INFO(STB_GLOBAL,STT_OBJECT);
    sym.st_name=addStrtabEntryToExisting(patchedBin,var->name,false);
    sym.st_shndx=elf_ndxscn(getSectionByName(patchedBin,".data.new"));
    sym.st_value=var->newLocation;
    addSymtabEntryToExisting(patchedBin,&sym);
  }
}

void applyFunctionPatch(SubprogramInfo* func,int pid,ElfInfo* targetBin,ElfInfo* patch)
{
  logprintf(ELL_INFO_V2,ELS_PATCHAPPLY,"patching function %s\n",func->name);
  //int len=func->highpc-func->lowpc;
  uint offset=func->lowpc;//-patch->textStart[IN_MEM]
  //get the function text from the patch file
  //void* data=getTextDataAtRelOffset(patch,func->lowpc);
  addr_t addr=patchTextAddr+offset;
  printf("patch text address is 0x%x\n",(uint)patchTextAddr);

  //not doing copy any more, already did that when mapped in
  //the whole text segment at once
  //memcpyToTarget(addr,data,len);
  
  //now look up the symbol in the old binary to discover where to insert
  //the trampoline jump
  int idx=getSymtabIdx(targetBin,func->name,0);
  if(idx!=STN_UNDEF)
  {
    addr_t oldAddr=getSymAddress(targetBin,idx);
    GElf_Sym sym;
    getSymbol(targetBin,idx,&sym);
    sym.st_value=addr;
    //now we write the symbol to the new binary to keep
    //track of where it is for future patches
    Elf_Data* symTabData=getDataByERS(patchedBin,ERS_SYMTAB);
    gelf_update_sym(symTabData,idx,&sym);
    insertTrampolineJump(oldAddr,addr);

  }
  else
  {
    //the function did not exist previously!
    //we create a new symbol for it
    //todo: really should abstract this out
    ElfXX_Sym sym;
    memset(&sym,0,sizeof(ElfXX_Sym));
    //todo: might not always be the case that it's global
    sym.st_info=ELFXX_ST_INFO(STB_GLOBAL,STT_FUNC);
    sym.st_name=addStrtabEntryToExisting(patchedBin,func->name,false);
    sym.st_shndx=elf_ndxscn(getSectionByName(patchedBin,".text.new"));
    sym.st_value=addr;
    addSymtabEntryToExisting(patchedBin,&sym);
  }
}


int addStrtabEntryToExisting(ElfInfo* e,char* str,bool header)
{
  
  Elf_Scn* scn;
  if(header)
  {
    scn=elf_getscn(e->e,e->sectionHdrStrTblIdx);
  }
  else
  {
    scn=elf_getscn(e->e,e->strTblIdx);
  }
  Elf_Data* data=elf_getdata(scn,NULL);
  int newSize=data->d_size+strlen(str)+1;
  data->d_buf=realloc(data->d_buf,newSize);
  memcpy((char*)data->d_buf+data->d_size,str,strlen(str)+1);
  int retval=data->d_size;
  data->d_size=newSize;
  elf_flagdata(data,ELF_C_SET,ELF_F_DIRTY);
  GElf_Shdr shdr;
  gelf_getshdr(scn,&shdr);
  shdr.sh_size=data->d_size;
  gelf_update_shdr(scn,&shdr);
  return retval;
}

int addSymtabEntryToExisting(ElfInfo* e,ElfXX_Sym* sym)
{
  Elf_Data* data=getDataByERS(e,ERS_SYMTAB);
  int len=sizeof(ElfXX_Sym);
  int newSize=data->d_size+len;
  data->d_buf=realloc(data->d_buf,newSize);
  memcpy((char*)data->d_buf+data->d_size,sym,len);
  data->d_size=newSize;
  e->symTabCount=newSize/len;
  //printf("symtab count is now %i\n",e->symTabCount);
  elf_flagdata(data,ELF_C_SET,ELF_F_DIRTY);
  return e->symTabCount-1;
}


//copies the section with name from patch into patchedBin with name newName
//if newName is NULL, it will be taken to be the same as name
addr_t copyInEntireSection(ElfInfo* patch,char* name,char* newName)
{
  //todo: make sure copy in section word-aligned. Maybe even page
  //aligned? I don't think the latter is required though
  if(!newName)
  {
    newName=name;
  }
  //todo: perhaps might make more sense to mmap that part of the file?
  Elf_Scn* scn=getSectionByName(patch,name);
  if(!scn)
  {
    death("Failed to find section %s in patch\n",name);
  }
  Elf_Data* data=elf_getdata(scn,NULL);
  if(!data)
  {
    death("Failed to find data for section %s in patch\n",name);
  }
  addr_t addr=getFreeSpaceInTarget(data->d_size);
  if(data->d_size)
  {
    logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"mapping in the entirety of %s Copying %li bytes to 0x%lx\n",name,(long)data->d_size,(unsigned long)addr);
    memcpyToTarget(addr,data->d_buf,data->d_size);
  }
  else
  {
    logprintf(ELL_WARN,ELS_PATCHAPPLY,"Section %s does not contain any data, so cannot map it in\n",name);
  }

  //and create a section for it
  Elf_Scn* newscn = elf_newscn (patchedBin->e);
  GElf_Shdr shdr,shdrNew;
  gelf_getshdr(scn,&shdr);
  gelf_getshdr(scn,&shdrNew);
  shdrNew.sh_addr=addr;
  shdrNew.sh_name=addStrtabEntryToExisting(patchedBin,newName,true);
  shdrNew.sh_type=shdr.sh_type;
  shdrNew.sh_flags=shdr.sh_flags | SHF_ALLOC;
  shdrNew.sh_size=shdr.sh_size;
  shdrNew.sh_link=0;//todo: should this be set?
  shdrNew.sh_info=0;//todo: should this be set?
  shdrNew.sh_addralign=shdr.sh_addralign;
  shdrNew.sh_entsize=shdr.sh_entsize;
  gelf_update_shdr(newscn,&shdrNew);
  Elf_Data* newdata=elf_newdata(newscn);
  *newdata=*data;
  newdata->d_buf=zmalloc(newdata->d_size);
  memcpy(newdata->d_buf,data->d_buf,data->d_size);

  //add a symbol
  ElfXX_Sym sym;
  memset(&sym,0,sizeof(ElfXX_Sym));
  sym.st_info=ELFXX_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_name=0;//traditionally section symbols have no name (I dunno why) and
  //keeping this consistency helps in reindexing symbols
  sym.st_shndx=elf_ndxscn(newscn);
  sym.st_value=addr;
  addSymtabEntryToExisting(patchedBin,&sym);
  return addr;
}


//this is a horrible function full of hacks that I've been trying to
//get working and have been failing it. It should be massively
//revamped before it's any good basically it attempts to fix the
//program header so the program can run, but doesn't do so
//successfully
void writeOutPatchedBin(bool flushToDisk)
{
  //todo: this is all not working
  #ifdef not_defined
  //need this if using ELF_F_LAYOUT due to bug(?) in libelf
  //leading to incorrect program headers if we let libelf
  //handle the layout
  GElf_Ehdr ehdr;
  if(!gelf_getehdr(patchedBin->e,&ehdr))
  {
    death("failed to get ehdr in fixupPatchedBinSectionHeaders\n");
  }
  int offsetSoFar=0;
  for(int i=1;i<ehdr.e_shnum;i++)
  {
    Elf_Scn* scn=elf_getscn(patchedBin->e,i);
    assert(scn);
    GElf_Shdr shdr;
    if(!gelf_getshdr(scn,&shdr))
    {
      death("failed to get shdr in fixupPatchedBinSectionHeaders\n");
    }
    //hack here. Assuming we won't be modifying the first section and
    //can get the likely offset of all the others starting from the
    //first section (whose offset depends on the headers coming before
    //it)
    int alignBytes=offsetSoFar%shdr.sh_addralign;
    if(alignBytes)
    {
      offsetSoFar+=shdr.sh_addralign-alignBytes;//complement it to get
                                              //what we actually need
                                              //to increase the offset
                                              //by
    }
    printf("old offset for section %i is %i and offset so far is %u\n",i,(int)shdr.sh_offset,offsetSoFar);
    if(1==i)
    {
      offsetSoFar=shdr.sh_offset;
    }
    else
    {
      //shdr.sh_offset=offsetSoFar;
    }
    
    Elf_Data* data=elf_getdata(scn,NULL);
    if(data)
    {
      shdr.sh_size=data->d_size;
    }
    offsetSoFar+=shdr.sh_size;
    gelf_update_shdr(scn,&shdr);
  }
  #endif
  
  //just mark everything as dirty, easier than keeping track on low level
  //todo: as optimization could keep track at low level
  elf_flagelf(patchedBin->e,ELF_C_SET,ELF_F_DIRTY);

  elf_flagelf(patchedBin->e,ELF_C_CLR,ELF_F_LAYOUT);
  if(elf_update(patchedBin->e, ELF_C_NULL) <0)
  {
    fprintf(stderr,"Failed to write out patched elf file representing mem: %s\n",elf_errmsg (-1));
    death(NULL);
  }
  GElf_Ehdr ehdr;
  if(!gelf_getehdr(patchedBin->e,&ehdr))
  {
    fprintf(stdout,"Failed to get ehdr from elf file we're duplicating: %s\n",elf_errmsg (-1));
    death(NULL);
  }
  if(ehdr.e_phnum > 0)
  {
    int cnt;
    for(cnt = 0;cnt < ehdr.e_phnum;++cnt)
    {
      GElf_Phdr phdr_mem;
      if(!gelf_getphdr(targetBin->e,cnt,&phdr_mem))
      {
        fprintf(stdout,"Failed to get phdr from elf file we're duplicating: %s\n",elf_errmsg (-1));
        death(NULL);
      }
      gelf_update_phdr(patchedBin->e,cnt,&phdr_mem);
    }
  }

  //hackish fix for now, get this more thoroughly in the future
  GElf_Phdr phdr;
  gelf_getphdr(patchedBin->e,2,&phdr);
  GElf_Shdr shdr;
  Elf_Scn* scn=getSectionByName(patchedBin,".eh_frame");
  gelf_getshdr(scn,&shdr);
  phdr.p_filesz=shdr.sh_offset+shdr.sh_size;
  phdr.p_memsz=0x888;//horrible hack specific to what we're debugging now
  gelf_update_phdr(patchedBin->e,2,&phdr);

  gelf_getphdr(patchedBin->e,3,&phdr);
  scn=getSectionByName(patchedBin,".ctors");
  gelf_getshdr(scn,&shdr);
  phdr.p_offset=shdr.sh_offset;
  scn=getSectionByName(patchedBin,".bss");
  gelf_getshdr(scn,&shdr);
  phdr.p_filesz=shdr.sh_offset+shdr.sh_size-phdr.p_offset;
  gelf_update_phdr(patchedBin->e,3,&phdr);

  gelf_getphdr(patchedBin->e,4,&phdr);
  scn=getSectionByName(patchedBin,".dynamic");
  gelf_getshdr(scn,&shdr);
  phdr.p_offset=shdr.sh_offset;
  phdr.p_filesz=shdr.sh_offset+shdr.sh_size-phdr.p_offset;
  gelf_update_phdr(patchedBin->e,4,&phdr);

  gelf_getphdr(patchedBin->e,7,&phdr);
  scn=getSectionByName(patchedBin,".ctors");
  gelf_getshdr(scn,&shdr);
  phdr.p_offset=shdr.sh_offset;
  scn=getSectionByName(patchedBin,".got");
  gelf_getshdr(scn,&shdr);
  phdr.p_filesz=shdr.sh_offset+shdr.sh_size-phdr.p_offset;
  gelf_update_phdr(patchedBin->e,7,&phdr);

  elf_flagelf(patchedBin->e,ELF_C_SET,ELF_F_DIRTY);
  elf_flagelf(patchedBin->e,ELF_C_SET,ELF_F_LAYOUT);
  //now write out our patched binary
  if(elf_update(patchedBin->e, flushToDisk?ELF_C_WRITE:ELF_C_NULL) <0)
  {
    fprintf(stderr,"Failed to write out patched elf file representing mem: %s\n",elf_errmsg (-1));
    death(NULL);
  }
}


//called once the sections have been mapped from the patch into
//the binary so that relocations are correct: refer to the correct
//symbol indices and PC relative relocations are ok
void fixupPatchRelocations(ElfInfo* patch)
{
  Elf_Scn* relTextScn=getSectionByName(patch,".rela.text.new");
  //go through and reindex all of the symbols
  Elf_Data* data=elf_getdata(relTextScn,NULL);
  int numRelocs=data->d_size/sizeof(ElfXX_Rela);
  Elf_Scn* patchTextNew=getSectionByName(patch,".text.new");
  GElf_Shdr shdr;
  if(!gelf_getshdr(patchTextNew,&shdr))
  {
    death("failed to get shdr\n");
  }
  
  addr_t oldTextNewStart=shdr.sh_addr;
  printf("old start is 0x%x\n",(uint)oldTextNewStart);

  Elf_Scn* textScn=getSectionByName(patch,".text.new");
  Elf_Data* textData=elf_getdata(textScn,NULL);
 
  for(int i=0;i<numRelocs;i++)
  {
    ElfXX_Rela* rela=((ElfXX_Rela*)data->d_buf)+i;
    int symIdx=ELFXX_R_SYM(rela->r_info);
    int type=ELFXX_R_TYPE(rela->r_info);
    GElf_Sym sym;
    getSymbol(patch,symIdx,&sym);

    int flags=ESFF_MANGLED_OK | ESFF_BSS_MATCH_DATA_OK;
    if(ELF64_ST_TYPE(sym.st_info)!=STT_SECTION)
    {
      flags|=ESFF_VERSIONED_SECTIONS_OK;
    }
    int reindex=reindexSymbol(patch,patchedBin,symIdx,flags);
    logprintf(ELL_INFO_V2,ELS_SYMBOL,"reindexed to %i at 0x%x\n",reindex,(uint)getSymAddress(patchedBin,reindex));
    if(STN_UNDEF==reindex)
    {
      death("Could not reindex symbol from patch to patchedBin\n");
    }
    rela->r_info=ELFXX_R_INFO(reindex,type);
    addr_t newOffset=rela->r_offset-oldTextNewStart+patchTextAddr;

    //fix up PC32-relocations so we keep the same absolute address
    //hopefully most of them will be properly fixed up by PLT or GOT
    //todo: even allow this at all? Issue warning later
    //if some not taken care of?
    if(type==R_386_PC32 || type==R_X86_64_PC32)
    {
      assert(rela->r_offset+sizeof(uint32) <= textData->d_size);
      addr_t addrAccessed=(addr_t)*((uint32*)(textData->d_buf+rela->r_offset));
      //if we were using a PC-relative relocation and the PC wasn't in the patch text,
      //have to update it for this relocation
      //todo: don't do this if the relocation was actually
      //for something relative to the patch
      addr_t diff=patchTextAddr-oldTextNewStart;
      logprintf(ELL_INFO_V2,ELS_RELOCATION,"for PC32 relocation, modifying access at 0x%x to access 0x%x by adding 0x%x\n",(uint)newOffset,(uint)(addrAccessed+diff),(uint)diff);
      addr_t newAddr=addrAccessed+diff;
        
      memcpyToTarget(newOffset,(byte*)&newAddr,4);//always copy only 4 bytes b/c this is PC32
        //todo: I'm not sure this is necessary here, since we do
        //relocations later. I think it is though. Look into this
    }
    if(R_X86_64_PC64==type)
    {
      death("Support for R_X86_64_PC64 still needs to be implemented. Please file a bug report\n");
    }
    rela->r_offset=newOffset;
  }
}

//copy PLT and GOT to new locations, as we may need to expand them
///todo: this function may not work for x86-64 Large Code Model
void katanaPLT()
{
  //todo: need to handle this differently if patching
  //something that's already been patched
  
  //we do the following
  //1. copy .plt, .got, and .got.plt into new locations (%.katana)
  //2. copy .rel(a).plt and .rel(a).rel(a).plt into new locations (%.katana)
  //   rel(a).rel(a).plt not implemented yet because I don't know what it does
  //3. fix up relocations/addresses as necessary
  //currently we copy them without any changes. The purpose of this is so that
  //32-bit calls in .text/etc will work correctly after we've relocated other
  //sections. Eventually we will also allow adding in new symbols

  GElf_Shdr shdr;
  getShdrByERS(targetBin,ERS_GOT,&shdr);
  addr_t oldGOTAddress=shdr.sh_addr;
  addr_t oldGOTPLTAddress=0;
  addr_t newGOTPLTAddress=0;
  word_t oldGOTPLTSize=0;

  //////////////////////////////////////////////////
  //copy in the PLT section so other sections can reference it
  getShdrByERS(targetBin,ERS_PLT,&shdr);
  #ifdef KATANA_X86_64_ARCH
  addr_t oldPLTAddress=shdr.sh_addr;
  #endif
  copyInEntireSection(targetBin,".plt",".plt.katana");
  Elf_Scn* pltScn=getSectionByName(patchedBin,".plt.katana");
  assert(pltScn);
  patchedBin->sectionIndices[ERS_PLT]=elf_ndxscn(pltScn);

  
  //////////////////////////////////////////////////////////////////
  //deal with GOT/PLTGOT stuff
  //copy in .got and .got.plt. We will not have to do anything strange
  //to these sections: they contain absolute addresses
  //it is my understanding (based on p. 47 of the Elf Format Reference at
  //http://www.skyfree.org/linux/references/Elf_Format.pdf) that the first
  //entry will always be the address of the .dynamic section. I am not sure
  //whether this resides in .got or .got.plt
  //todo: figure out what does reside in .got if .got.plt exists
  addr_t newGOTAddress=copyInEntireSection(targetBin,".got",".got.katana");
  Elf_Scn* gotScn=getSectionByName(patchedBin,".got.katana");
  assert(gotScn);
  patchedBin->sectionIndices[ERS_GOT]=elf_ndxscn(gotScn);
  if(getSectionByName(targetBin,".got.plt"))
  {
    getShdrByERS(targetBin,ERS_GOTPLT,&shdr);
    oldGOTPLTAddress=shdr.sh_addr;
    oldGOTPLTSize=shdr.sh_size;
    newGOTPLTAddress=copyInEntireSection(targetBin,".got.plt",".got.plt.katana");
    Elf_Scn* gotPltScn=getSectionByName(patchedBin,".got.plt.katana");
    assert(gotPltScn);
    patchedBin->sectionIndices[ERS_GOTPLT]=elf_ndxscn(gotPltScn);
  }

  ////////////////////////////////////////////////////////////////
  //deal with RELPLT stuff
  char* pltRelScnName=".rela.plt";
  Elf_Scn* pltRelScn=getSectionByName(targetBin,pltRelScnName);
  if(!pltRelScn)
  {
    pltRelScnName=".rel.plt";
    pltRelScn=getSectionByName(targetBin,pltRelScnName);
  }
  Elf_Scn* originalPltRelScn=pltRelScn;
  if(pltRelScn)
  {
    char buffer[128];
    snprintf(buffer,128,"%s.katana",pltRelScnName);
    copyInEntireSection(targetBin,pltRelScnName,buffer);
    pltRelScn=getSectionByName(patchedBin,buffer);
    assert(pltRelScn);
    patchedBin->sectionIndices[ERS_RELX_PLT]=elf_ndxscn(pltRelScn);
    //now have to make this section reference our new plt section, not the old plt section
    getShdr(pltRelScn,&shdr);
    shdr.sh_link=patchedBin->sectionIndices[ERS_SYMTAB];
    shdr.sh_info=elf_ndxscn(pltScn);
    int numRelocations=shdr.sh_size/shdr.sh_entsize;
    gelf_update_shdr(pltRelScn,&shdr);
    //.rela.plt does not actually exist to relocate addresses in the PLT
    //It provides relocation entries which describe how to get to GOT entries.
    //so what we need to do now to make .rela.plt.katana behave properly
    //is change the r_offset values to make it refer to entries in .got.plt.katana (if it exists)
    //or .got.katana if it does not
    //todo: should perhaps read the PLTGOT entry in .dynamic for this?
    //      or maybe the symbol _GLOBAL_OFFSET_TABLE_?
    //      It's unclear from the ABI
    addr_t oldAddress=oldGOTPLTAddress;
    addr_t newAddress=newGOTPLTAddress;
    if(!oldAddress)
    {
      oldAddress=oldGOTAddress;
      newAddress=newGOTAddress;
    }
    int scnIdx=elf_ndxscn(originalPltRelScn);
    for(int i=0;i<numRelocations;i++)
    {
      RelocInfo* reloc=getRelocationEntryAtOffset(patchedBin,originalPltRelScn,i*shdr.sh_entsize);
      //todo: should we check the type are are we just assuming it's always JUMP_SLOT
      reloc->r_offset=reloc->r_offset-oldAddress+newAddress;
      reloc->scnIdx=scnIdx;
      setRelocationEntryAtOffset(reloc,pltRelScn,i*shdr.sh_entsize);
    }
  }
  //todo: what do we need to do with .rela.rela.plt? What is it for?
  //      doesn't seem to be mentioned in any standards

  ////////////////////////////////////////////////////
  //actually deal with the PLT
  getShdr(pltScn,&shdr);
  //it contains lots of references to the GOT, which have now changed
  Elf_Data* pltData=elf_getdata(pltScn,NULL);
  //PLT0 is special, we fix it up first
  //todo: support large code model under x86_64
  //uint32 here is applicable for small and medium
  uint32_t addr=newGOTAddress-(shdr.sh_addr+6); //subtraction because relative addressing
#ifdef KATANA_X86_ARCH
  addr+=4;
#elif defined(KATANA_X86_64_ARCH)
    addr+=8;
#else
#error Unknown architecture
#endif
  memcpyToTarget(shdr.sh_addr+2,(byte*)&addr,sizeof(addr));
  memcpy(pltData->d_buf+2,&addr,sizeof(addr));
  //now fix up the jmp in PLT0
  addr=newGOTAddress-(shdr.sh_addr+12); //subtraction because relative addressing
#ifdef KATANA_X86_ARCH
  addr+=8;
#elif defined(KATANA_X86_64_ARCH)
  addr+=16;
#endif
  memcpyToTarget(shdr.sh_addr+8,(byte*)&addr,sizeof(addr));
  memcpy(pltData->d_buf+2,&addr,sizeof(addr));
  //now we've fixed up PLT0,
  //proceed to the rest
  uint pltEntsize=shdr.sh_entsize;
  #ifdef KATANA_X86_ARCH
  //will always be the case, since it doesn't vary on x86
  //gcc seems to set it to 4, which seems incorrect by any metric
  //I can think of. gcc on x86_64 sets it in a sensible manner
  pltEntsize=0x10;
  #endif

  int numPLTEntries=shdr.sh_size/pltEntsize;
  for(int i=1;i<numPLTEntries;i++)
  {
    //even though at present we only support small code model PLT,
    //use full sized addresses internally because we turn relative addresses
    //into absolute ones, and want to make sure we don't lose any information
    addr_t oldAddr=0;//old address of the GOT entry corresponding to this plt entry
    word_t entryOffset=i*pltEntsize;
    //todo: support x86_64 large code model where will be 8 bytes,
    //not 4
    memcpy(&oldAddr,pltData->d_buf+entryOffset+2,4);
    #ifdef KATANA_X86_64_ARCH
    oldAddr+=oldPLTAddress+entryOffset+6;//was a PC-relative address
    #endif

    //create newAddr as an absolute address, the new address of the
    //GOT entry corresponding to this PLT entry
    addr_t newAddr=oldAddr-oldGOTAddress+newGOTAddress;

#ifdef KATANA_X86_64_ARCH
    //make it a pc-relative address again
    newAddr-=shdr.sh_addr+entryOffset+6;
#endif
    memcpyToTarget(shdr.sh_addr+entryOffset+2,(byte*)&newAddr,4);//todo: support large code model
    memcpy(pltData->d_buf+entryOffset+2,&newAddr,4);
  }

}

void readAndApplyPatch(int pid,ElfInfo* targetBin_,ElfInfo* patch)
{
  startPtrace(pid);
  targetBin=targetBin_;
  
  
  //we create an on-disk version of the patched binary
  //setting this up is much easier than modifying the in-memory ELF
  //structures. It does, however, allow us to write an accurate symbol table,
  //relocation info, etc. This allows applying a patch to
  //an executable that's already had a patch applied to it.
  int version=calculateVersionAfterPatch(pid,patch);
  char* dir=createKatanaDirs(pid,version);
  char patchedBinFname[256];
  snprintf(patchedBinFname,256,"%s/exe",dir);
  free(dir);
  elf_flagelf(targetBin->e,ELF_C_SET,ELF_F_LAYOUT);
  patchedBin=duplicateElf(targetBin,patchedBinFname,false,true);
  
  elf_flagelf(patchedBin->e,ELF_C_SET,ELF_F_LAYOUT);//we assume all responsibility
  //for layout. For some reason libelf seems to have issues with some of the program
  //headers such that the program can't execute if we don't do this.
  //I suspect either a bug in libelf or something wrong with my own usage, but
  //I have no idea what I might be doing wrong and don't have the patience to debug libelf
  //right now, so we'll just work around this by doing layout manually
  //see http://uw713doc.sco.com/en/man/html.3elf/elf_update.3elf.html (or some
  //other manpage on elf_update) for a description of what we now
  //have to take care of manually
  //note: this actually isn't working, I can't get the ELF
  //file written out in such a way that it executes. I need to figure
  //this out more

  //go through variables in the patch file and apply their fixups
  //todo: we're assuming for now that symbols in the binary the patch was generated
  //with and in the target binary are going to have the same values
  //this isn't necessarily going to be the case
  char cwd[PATH_MAX];
  getcwd(cwd,PATH_MAX);
  DwarfInfo* diPatch=readDWARFTypes(patch,cwd);
  Map* fdeMap=readDebugFrame(patch,false);//get mapping between fde offsets and fde structures
  if(!fdeMap)
  {
    death("Unable to read frame info, can't apply patch\n");
  }
  


  //we need to know where malloc lives in the target because
  //we may need it when dealing with the heap
  addr_t mallocAddr=locateRuntimeSymbolInTarget(targetBin,"malloc");
  if(mallocAddr)
  {
    setMallocAddress(mallocAddr);
  }
  else
  {
    death("Cannot find malloc in the target program\n");
  }
  setTargetTextStart(targetBin->textStart[IN_MEM]);

  
  bringTargetToSafeState(targetBin,patch,pid);

  //reserve memory in a big block so that we'll have as much as we need
  uint amount=0;
  GElf_Shdr shdr;
  char* sectionsToMapIn[]={".text.new",".rodata.new",".data.new",".rela.text.new",NULL};
  for(int i=0;sectionsToMapIn[i];i++)
  {
    getShdr(getSectionByName(patch,sectionsToMapIn[i]),&shdr);
    amount+=shdr.sh_size;
  }
  //include their sizes so we can use ALTPLT/EXTPLT technique from ERESI/Elfsh
  getShdrByERS(targetBin,ERS_GOT,&shdr);
  amount+=shdr.sh_size;
  getShdrByERS(targetBin,ERS_PLT,&shdr);
  amount+=shdr.sh_size;
  getShdrByERS(targetBin,ERS_GOTPLT,&shdr);
  amount+=shdr.sh_size;

  #ifdef KATANA_X86_64_ARCH
  addr_t desiredAddress=0;

  //we need to request a memory address near the beginning
  //of the binary to accomodate small code model
  //TODO: make sure this is scalable, that I've taken
  //all possibilities into account
  MappedRegion* regions=0;
  int numRegions=getMemoryMap(pid,&regions);
  if(numRegions>=2)
  {
    desiredAddress=(regions[1].high+1+sizeof(word_t));
    uint misalignment=desiredAddress%sizeof(word_t);
    desiredAddress-=misalignment;
  }
  #endif

  #ifdef KATANA_X86_64_ARCH
  addr_t receivedAddres=reserveFreeSpaceInTarget(amount,desiredAddress);
  if(receivedAddres > 0xFFFFFFFF && patchedBin->textUsesSmallCodeModel)
  {
    //todo: if at first we don't succeed, try again
    death("Needed to put new memory pages in the lower 32 bits of the address space and was unable to accomplish this");
  }
  #endif
    

  //map in the entirety of .text.new
  patchTextAddr=copyInEntireSection(patch,".text.new",NULL);

  //map in entirety of .rodata.new
  patchRodataAddr=copyInEntireSection(patch,".rodata.new",NULL);

  patchDataAddr=copyInEntireSection(patch,".data.new",NULL);

  //todo: now that we're trying to copy everything into the lower 32-bits even if
  //on x86_64, this isn't really necessary, is it?
  katanaPLT();
  
  writeOutPatchedBin(false);

  logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"======Applying patches=======\n");
  for(List* cuLi=diPatch->compilationUnits;cuLi;cuLi=cuLi->next)
  {
    CompilationUnit* cu=cuLi->value;
    printf("reading patch compilation unit %s\n",cu->name);
        
    //first patch variables
    VarInfo** vars=(VarInfo**) dictValues(cu->tv->globalVars);
    for(int i=0;vars[i];i++)
    {
      applyVariablePatch(vars[i],fdeMap,patch);
    }
    free(vars);

    //then patch functions
    SubprogramInfo** subprograms=(SubprogramInfo**)dictValues(cu->subprograms);
    for(int i=0;subprograms[i];i++)
    {
      applyFunctionPatch(subprograms[i],pid,targetBin,patch);
    }
    free(subprograms);
  }


  mapDelete(fdeMap,NULL,free);
  logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"======Fixup Patch Relocations=======\n");
  fixupPatchRelocations(patch);
  logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"====================================\n");
  patchRelTextAddr=copyInEntireSection(patch,".rela.text.new",NULL);

  writeOutPatchedBin(false);

  logprintf(ELL_INFO_V1,ELS_PATCHAPPLY,"======Performing Patch Relocations=======\n");
    
  //now perform relocations to our functions to give them a chance
  //of working
  Elf_Scn* relTextScn=getSectionByName(patchedBin,".rela.text.new");
  Elf_Data* data=elf_getdata(relTextScn,NULL);
  
  RelocInfo reloc;
  memset(&reloc,0,sizeof(reloc));
  reloc.e=patchedBin;
  reloc.scnIdx=elf_ndxscn(getSectionByName(patchedBin,".text.new"));
  GElf_Rela rela;
  int numRelocs=data->d_size/sizeof(ElfXX_Rela);
  for(int i=0;i<numRelocs;i++)
  {
    if(!gelf_getrela(data,i,&rela))
    {
      death("Failed to get relocation\n");
    }
    reloc.r_offset=rela.r_offset;
    reloc.r_addend=rela.r_addend;
    reloc.relocType=ELF64_R_TYPE(rela.r_info);//elf64 because it's GElf
    reloc.symIdx=ELF64_R_SYM(rela.r_info);//elf64 because it's GElf
    applyRelocation(&reloc,IN_MEM);//todo: on disk as well
  }

  writeOutPatchedBin(true);
  endELF(targetBin);
  endELF(patchedBin);
  cleanupDwarfVM();
  endPtrace(isFlag(EKCF_P_STOP_TARGET));
  printf("hooray! completed application of patch successfully\n");
}
