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

ElfInfo* patchedBin=NULL;
addr_t patchTextAddr=0;
addr_t patchRodataAddr=0;

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

void transformVarData(VarInfo* var,Map* fdeMap,ElfInfo* targetBin)
{
  printf("transforming var %s\n",var->name);
  FDE* transformerFDE=mapGet(fdeMap,&var->type->fde);
  if(!transformerFDE)
  {
    //todo: roll back atomically
    death("could not find transformer for variable\n");
  }
  patchDataWithFDE(var,transformerFDE,targetBin);
}

void relocateVar(VarInfo* var,ElfInfo* targetBin)
{
  //record in the patched binary that we're putting the variable here
  int symIdx=getSymtabIdx(targetBin,var->name);
  GElf_Sym sym;
  getSymbol(targetBin,symIdx,&sym);
  sym.st_value=var->newLocation;
  //now we write the symbol to the new binary
  gelf_update_sym(patchedBin->symTabData,symIdx,&sym);
  
  //todo: how much do we need to do as long as we're patching code
  //at the moment not patching code
  performRelocations(targetBin,var);
}

void insertTrampolineJump(addr_t insertAt,addr_t jumpTo)
{
  //remember that the JMP absolute is indirect, have to specify
  //memory location which hold the memory location to jump to
  int len=2+sizeof(addr_t)*2;
  byte *code=zmalloc(len);
  code[0]=0xFF;//jmp instruction for a near absolute jump
  code[1]=0x25;//specify the addressing mode

  addr_t addrAddr=insertAt+2+sizeof(addr_t);//address of mem location holding jmp target
  //code[2]=(addrAddr>>24)&0xFF;
  //code[3]=(addrAddr>>16)&0xFF;
  //code[4]=(addrAddr>>8)&0xFF;
  //code[5]=(addrAddr>>0)&0xFF;
  memcpy(code+2,&addrAddr,sizeof(addr_t));
  //todo: did I get endianness right here?
  //x86 is bizarre
  memcpy(code+2+sizeof(addr_t),&jumpTo,sizeof(addr_t));
  printf("inserting at 0x%x, address to jump to is 0x%x\n",(uint)insertAt,(uint)jumpTo);
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
  //int len=func->highpc-func->lowpc;
  uint offset=func->lowpc;//-patch->textStart[IN_MEM]
  //get the function text from the patch file
  //void* data=getTextDataAtRelOffset(patch,func->lowpc);
  addr_t addr=patchTextAddr+offset;

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
  gelf_update_sym(patchedBin->symTabData,idx,&sym);
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
  Elf_Data* data=e->symTabData;
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
  GElf_Shdr shdr;
  gelf_getshdr(scn,&shdr);
  shdr.sh_addr=patchTextAddr;
  shdr.sh_name=addStrtabEntryToExisting(patchedBin,name,true);
  gelf_update_shdr(newscn,&shdr);
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

void readAndApplyPatch(int pid,ElfInfo* targetBin,ElfInfo* patch)
{
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
  patchedBin=duplicateElf(targetBin,patchedBinFname,false);
  
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

  if(elf_update(patchedBin->e, ELF_C_NULL) <0)
  {
    fprintf(stderr,"Failed to update patched elf file representing mem: %s\n",elf_errmsg (-1));
    death(NULL);
  }

  Elf_Scn* relTextScn=getSectionByName(patch,".rela.text.new");
  //go through and reindex all of the symbols
  Elf_Data* data=elf_getdata(relTextScn,NULL);
  int numRelocs=data->d_size/sizeof(Elf32_Rela);
  for(int i=0;i<numRelocs;i++)
  {
    Elf32_Rela* rela=((Elf32_Rela*)data->d_buf)+i;
    int symIdx=ELF32_R_SYM(rela->r_info);
    int type=ELF32_R_TYPE(rela->r_info);
    int reindex=reindexSymbol(patch,patchedBin,symIdx);
    printf("reindexed to %i\n",reindex);
    rela->r_info=ELF32_R_INFO(reindex,type);
  }
  
  addr_t patchRelTextAddr=copyInEntireSection(patch,".rela.text.new",&trans);

  
  for(List* cuLi=diPatch->compilationUnits;cuLi;cuLi=cuLi->next)
  {
    CompilationUnit* cu=cuLi->value;
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
      transformVarData(vars[i],fdeMap,targetBin);
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

  //just mark everything as dirty, easier than keeping track on low level
  //todo: as optimization could keep track at low level
  elf_flagelf(patchedBin->e,ELF_C_SET,ELF_F_DIRTY);
  elf_flagelf(patchedBin->e,ELF_C_CLR,ELF_F_LAYOUT);
  
  //now write out our patched binary
  if(elf_update(patchedBin->e, ELF_C_WRITE) <0)
  {
    fprintf(stderr,"Failed to write out patched elf file representing mem: %s\n",elf_errmsg (-1));
    death(NULL);
  }
  elf_end(patchedBin->e);
  close(patchedBin->fd);
  
  printf("completed application of patch successfully\n");
}
