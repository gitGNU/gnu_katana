/*
  File: patchread.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Read patch object files and apply them
*/

#include "elfparse.h"
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

ElfInfo* patchedBin=NULL;
ElfInfo* targetBin=NULL;
addr_t patchTextAddr=0;
addr_t patchRodataAddr=0;
addr_t patchRelTextAddr=0;

void allocateMemoryForVarRelocation(VarInfo* var,TransformationInfo* trans)
{
  printf("allocating memory for relocating variabl %s\n",var->name);
  int length=var->type->length;
  var->newLocation=getFreeSpaceForTransformation(trans,length);
  byte* zeros=zmalloc(length);
  printf("zeroing out new memory\n");
  memcpyToTarget(var->newLocation,zeros,length);
  free(zeros);
  //todo: handle errors
}

void transformVarData(VarInfo* var,Map* fdeMap,ElfInfo* patch)
{
  printf("transforming var %s\n",var->name);
  FDE* transformerFDE=mapGet(fdeMap,&var->type->fde);
  if(!transformerFDE)
  {
    //todo: roll back atomically
    death("could not find transformer for variable\n");
  }
  patchDataWithFDE(var,transformerFDE,targetBin,patch);
}

void relocateVar(VarInfo* var,ElfInfo* targetBin)
{
  //record in the patched binary that we're putting the variable here
  int symIdx=getSymtabIdx(targetBin,var->name);
  GElf_Sym sym;
  getSymbol(targetBin,symIdx,&sym);
  sym.st_value=var->newLocation;
  //now we write the symbol to the new binary
  Elf_Data* symTabData=getDataByERS(patchedBin,ERS_SYMTAB);
  gelf_update_sym(symTabData,symIdx,&sym);
  
  //todo: how much do we need to do as long as we're patching code
  //at the moment not patching code
  performRelocations(targetBin,var);
}

void insertTrampolineJump(addr_t insertAt,addr_t jumpTo)
{
  printf("inserting at 0x%x, address to jump to is 0x%x\n",(uint)insertAt,(uint)jumpTo);
  
  //remember that the JMP absolute is indirect, have to specify
  //memory location which hold the memory location to jump to
  int len=2+sizeof(addr_t)*2;
  byte *code=zmalloc(len);
  code[0]=0xFF;//jmp instruction for a near absolute jump
  code[1]=0x25;//specify the addressing mode

  addr_t addrAddr=insertAt+2+sizeof(addr_t);//address of mem location holding jmp target
  memcpy(code+2,&addrAddr,sizeof(addr_t));
  memcpy(code+2+sizeof(addr_t),&jumpTo,sizeof(addr_t));
  memcpyToTarget(insertAt,code,len);
  byte* verify=zmalloc(len);
  memcpyFromTarget(verify,insertAt,len);
  if(memcmp(code,verify,len))
  {
    death("failed to copy code into target properly\n");
  }
}



void applyFunctionPatch(SubprogramInfo* func,int pid,ElfInfo* targetBin,ElfInfo* patch,
                        TransformationInfo* trans)
{
  printf("patching function %s\n",func->name);
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
  //todo: return value checking
  int idx=getSymtabIdx(targetBin,func->name);
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

int addSymtabEntryToExisting(ElfInfo* e,Elf32_Sym* sym)
{
  Elf_Data* data=getDataByERS(e,ERS_SYMTAB);
  int len=sizeof(Elf32_Sym);
  int newSize=data->d_size+len;
  data->d_buf=realloc(data->d_buf,newSize);
  memcpy((char*)data->d_buf+data->d_size,sym,len);
  data->d_size=newSize;
  e->symTabCount=newSize/len;
  printf("symtab count is now %i\n",e->symTabCount);
  elf_flagdata(data,ELF_C_SET,ELF_F_DIRTY);
  return e->symTabCount-1;
}


addr_t copyInEntireSection(ElfInfo* patch,char* name,TransformationInfo* trans)
{
  //todo: perhaps might make more sense to mmap that part of the file?
  Elf_Scn* scn=getSectionByName(patch,name);
  Elf_Data* data=elf_getdata(scn,NULL);
  addr_t addr=getFreeSpaceForTransformation(trans,data->d_size);
  printf("mapping in the entirety of %s Copying %i bytes to 0x%x\n",name,data->d_size,(uint)addr);
  memcpyToTarget(addr,data->d_buf,data->d_size);
  //and create a section for it
  Elf_Scn* newscn = elf_newscn (patchedBin->e);
  GElf_Shdr shdr,shdrNew;
  gelf_getshdr(scn,&shdr);
  gelf_getshdr(scn,&shdrNew);
  shdrNew.sh_addr=addr;
  shdrNew.sh_name=addStrtabEntryToExisting(patchedBin,name,true);
  shdrNew.sh_type=shdr.sh_type;
  shdrNew.sh_flags=shdr.sh_flags;
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
  Elf32_Sym sym;
  memset(&sym,0,sizeof(Elf32_Sym));
  sym.st_info=ELF32_ST_INFO(STB_LOCAL,STT_SECTION);
  sym.st_name=0;//traditionally section symbols have no name (I dunno why) and
  //keeping this consistency helps in reindexing symbols
  sym.st_shndx=elf_ndxscn(newscn);
  sym.st_value=addr;
  addSymtabEntryToExisting(patchedBin,&sym);
  return addr;
}


//this is a horrible function full of hacks that I've been trying to get
//working and have been failing it. It should be massively revamped before it's any good
//basically it attempts to fix the program header so the program can run, but doesn't
//do so successfully
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

void readAndApplyPatch(int pid,ElfInfo* targetBin_,ElfInfo* patch)
{
  startPtrace();
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
  DwarfInfo* diPatch=readDWARFTypes(patch);
  Map* fdeMap=readDebugFrame(patch);//get mapping between fde offsets and fde structures

  TransformationInfo trans;
  memset(&trans,0,sizeof(trans));

  
  //map in the entirety of .text.new
  patchTextAddr=copyInEntireSection(patch,".text.new",&trans);

  //map in entirety of .rodata.new
  patchRodataAddr=copyInEntireSection(patch,".rodata.new",&trans);
  
  writeOutPatchedBin(false);

  
  Elf_Scn* relTextScn=getSectionByName(patch,".rela.text.new");
  //go through and reindex all of the symbols
  Elf_Data* data=elf_getdata(relTextScn,NULL);
  int numRelocs=data->d_size/sizeof(Elf32_Rela);
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
    Elf32_Rela* rela=((Elf32_Rela*)data->d_buf)+i;
    int symIdx=ELF32_R_SYM(rela->r_info);
    int type=ELF32_R_TYPE(rela->r_info);
    int reindex=reindexSymbol(patch,patchedBin,symIdx,ESFF_FUZZY_MATCHING_OK);
    printf("reindexed to %i at 0x%x\n",reindex,(uint)getSymAddress(patchedBin,reindex));
    if(STN_UNDEF==reindex)
    {
      death("Could not reindex symbol from patch to patchedBin\n");
    }
    rela->r_info=ELF32_R_INFO(reindex,type);
    addr_t newOffset=rela->r_offset-oldTextNewStart+patchTextAddr;

    //fix up PC32-relocations so we keep the same absolute address
    //hopefully most of them will be properly fixed up by PLT or GOT
    //todo: even allow this at all? Issue warning later
    //if some not taken care of?
    switch(type)
    {
    case R_386_PC32:
      {
        assert(rela->r_offset<textData->d_size);
        word_t addrAccessed=*((word_t*)(textData->d_buf+rela->r_offset));
        //if we were using a PC-relative relocation and the PC wasn't in the patch text,
        //have to update it for this relocation
        //todo: don't do this if the relocation was actually
        //for something relative to the patch
        addr_t diff=patchTextAddr-oldTextNewStart;
        printf("for PC32 relocation, modifying access at 0x%x to access 0x%x by adding 0x%x\n",(uint)newOffset,(uint)(addrAccessed+diff),(uint)diff);
        modifyTarget(newOffset,addrAccessed+diff);
      }
    }
    rela->r_offset=newOffset;
  }
  
  patchRelTextAddr=copyInEntireSection(patch,".rela.text.new",&trans);

  printf("======Applying patches=======\n");
  for(List* cuLi=diPatch->compilationUnits;cuLi;cuLi=cuLi->next)
  {
    CompilationUnit* cu=cuLi->value;
    printf("reading patch compilation unit %s\n",cu->name);
        
    //first patch variables
    VarInfo** vars=(VarInfo**) dictValues(cu->tv->globalVars);
    for(int i=0;vars[i];i++)
    {
      GElf_Sym sym;
      //todo: return value checking
      int idx=getSymtabIdx(targetBin,vars[i]->name);
      getSymbol(targetBin,idx,&sym);
      vars[i]->oldLocation=sym.st_value;
      bool relocate=sym.st_size < vars[i]->type->length;
      printf("need to relocated is %i as st_size is %i and new size is %i\n",relocate,(int)sym.st_size,vars[i]->type->length);
      if(relocate)
      {
        allocateMemoryForVarRelocation(vars[i],&trans);
      }
      transformVarData(vars[i],fdeMap,patch);
      if(relocate)
      {
        relocateVar(vars[i],targetBin);
      }
    }
    free(vars);

    //then patch functions
    SubprogramInfo** subprograms=(SubprogramInfo**)dictValues(cu->subprograms);
    for(int i=0;subprograms[i];i++)
    {
      applyFunctionPatch(subprograms[i],pid,targetBin,patch,&trans);
    }
  }


  //now perform relocations to our functions to give them a chance
  //of working
  relTextScn=getSectionByName(patchedBin,".rela.text.new");
  data=elf_getdata(relTextScn,NULL);
  
  RelocInfo reloc;
  memset(&reloc,0,sizeof(reloc));
  reloc.e=patchedBin;
  reloc.type=ERT_RELA;
  reloc.scnIdx=elf_ndxscn(getSectionByName(patchedBin,".text.new"));
  GElf_Rela rela;
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
    applyRelocation(&reloc,NULL,IN_MEM);//todo: on disk as well
  }

  writeOutPatchedBin(true);
  elf_end(patchedBin->e);
  close(patchedBin->fd);
  endPtrace();
  printf("completed application of patch successfully\n");
}
