/*
  File: patchwrite.c
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
  Description: Write patch information out to a po (patch object) file
*/

#include "types.h"
#include "dwarftypes.h"
#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <dwarf.h>
#include <assert.h>
#include "register.h"
#include "codediff.h"
#include "relocation.h"
#include "symbol.h"
#include "typediff.h"
#include "util/logging.h"
#include "elfwriter.h"
#include "sourcetree.h"
#include "write_to_dwarf.h"
#include "elfutil.h"

ElfInfo* oldBinary=NULL;
ElfInfo* newBinary=NULL;


ElfInfo* patch;
addr_t rodataOffset;//used to keep track of how much rodata we've used


Dwarf_P_Die firstCUDie=NULL;
Dwarf_P_Die lastCUDie=NULL;//keep track of for sibling purposes
Dwarf_P_Debug dbg;
Dwarf_Unsigned cie;//index of cie we're using



idx_t addSymbolFromBinaryToPatch(ElfInfo* binary,idx_t symIdx)
{
  assert(STN_UNDEF!=symIdx);
  //we expect this function to be called with binary equal to the
  //ELF object for one of the .o files. There may in fact, however,
  //be more information about the symbol in the new binary

  symIdx=reindexSymbol(binary,newBinary,symIdx,ESFF_FUZZY_MATCHING_OK);
  if(STN_UNDEF==symIdx)
  {
    death("could not reindex symbol from .o file to newBinary\n");
  }

  
  GElf_Sym symInNewBin;
  getSymbol(newBinary,symIdx,&symInNewBin);
  char* symname=elf_strptr(newBinary->e, newBinary->strTblIdx,
                           symInNewBin.st_name);
  //create a symbol to represent the symbol in newBinary
  //that this relocation refers to. If we don't already have a corresponding
  //symbol in the patch file, we may have to add this one
  ElfXX_Sym sym;
  sym.st_name=addStrtabEntry(patch,symname);//todo: this is a waste if we don't end up using this symbol
  sym.st_value=symInNewBin.st_value;
  sym.st_size=symInNewBin.st_size;
  sym.st_info=symInNewBin.st_info;
  int symType=ELF64_ST_TYPE(symInNewBin.st_info);
  sym.st_shndx=SHN_UNDEF;        
  if(STT_SECTION==symType)
  {
    sym.st_shndx=reindexSectionForPatch(newBinary,symInNewBin.st_shndx,patch);
  }
  sym.st_info=ELFXX_ST_INFO(ELF64_ST_BIND(symInNewBin.st_info),
                            symType);
  sym.st_other=symInNewBin.st_other;

  //we might have already added this same symbol to the patch
  GElf_Sym gsym=nativeSymToGELFSym(sym);
  int reindex=findSymbol(patch,&gsym,patch,false);
  if(SHN_UNDEF==reindex)
  {
    reindex=addSymtabEntry(patch,getDataByERS(patch,ERS_SYMTAB),&sym);
  }
  return reindex;
}

//segmentBase should be the start of the text that these relocations refer to
void writeRelocationsInRange(addr_t lowpc,addr_t highpc,Elf_Scn* scn,
                             addr_t segmentBase,ElfInfo* binary)
{
  List* relocs=getRelocationItemsInRange(binary,scn,lowpc,highpc);
  List* li=relocs;
  idx_t rodataScnIdx=elf_ndxscn(getSectionByERS(binary,ERS_RODATA));//for special handling of rodata because lump rodata from several binaries into one section
  for(;li;li=li->next)
  {
    //we always use RELA rather than REL in the patch file
    //because having the addend recorded makes some
    //things much easier to work with
    ElfXX_Rela rela;//what we actually write to the file
    RelocInfo* reloc=li->value;
    //todo: we insert symbols so that the relocations
    //will be valid, but we need to make sure we don't insert
    //a single symbol too many times, it wastes space
    int symIdx=reloc->symIdx;
    idx_t reindex=addSymbolFromBinaryToPatch(binary,symIdx);
    
    //now look at the relocation itself
    byte type=reloc->relocType;
    rela.r_offset=reloc->r_offset;
    //now rebase the offset based on where in the data this text is going
    GElf_Shdr shdr;
    gelf_getshdr(scn,&shdr);
    
    //note: this relies on our current incorrect usage of the offset field
    //for bookkeeping
    //todo: don't do that
    addr_t newRelOffset=rela.r_offset-lowpc+segmentBase;

    #ifdef legacy
    addr_t oldRelOffset=rela.r_offset;
    //now depending on the type we may have to do some additional fixups
    switch(type)
    {
    case R_386_PC32:
    case R_X86_64_PC32:
        {
          word_t addrAccessed=getWordAtAbs(elf_getscn(binary->e,reloc->scnIdx)
                                           ,oldRelOffset,IN_MEM);
          //this is a pc-relative relocation but we're changing the
          //address of the instruction. If we're not accessing
          //something inside the current function, then we might think
          //we actually want the type to be absolute, not relative. The problem
          //with that is often a PC32 relocation deals with a value at r_offset
          //that is not an address itself but an offset from the current address
          //and we must keep it that way, for example for relative calls
          //therefore we try to fix things up as best we can, although
          //in many cases we'll be dealing with something in the PLT,
          //and it will get relocated properly for that symbol
          //and we can ignore most of the relocation entry anyway
          if(addrAccessed < lowpc || addrAccessed > highpc)
          {
            addr_t diff=newRelOffset-oldRelOffset;
            //that doesn't look right, look into this. Why are we writing it
            //setWordAtAbs(elf_getscn(binary->e,reloc->scnIdx),oldRelOffset,addrAccessed+diff,IN_MEM);
          
            //we're not just accessing something in the current function
            //tweak the addend so that even with the new value
            //we can access the same absolute address, which is what
            //we really want to do. The text segment will never get
            //relocated on linux x86 so the reasons for making this
            //relative in the first place are gone
            //rela.r_addend=addrAccessed-(newRelOffset+1)-sym.st_value;
          }
        }
        break;
    }
    #endif

    rela.r_info=ELFXX_R_INFO(reindex,type);
    rela.r_addend=reloc->r_addend;


    //special handling for rodata, because each object has its own rodata
    //and we're no lumping them all together, we have to increase the addend
    //to fit where rodata actually goes
    GElf_Sym sym;
    if(!gelf_getsym(getDataByERS(binary,ERS_SYMTAB),symIdx,&sym))
    {death("gelf_getsym failed in writeRelocationsInRange\n");}
    if(sym.st_shndx==rodataScnIdx)
    {
      rela.r_addend+=rodataOffset;//increase it by the amount we're up to

    }
    //end special handling for rodata

 
    rela.r_offset=newRelOffset;
    logprintf(ELL_INFO_V4,ELS_RELOCATION,"adding reloc for offset 0x%x\n",rela.r_offset);
    addDataToScn(getDataByERS(patch,ERS_RELA_TEXT),&rela,sizeof(ElfXX_Rela));
  }
  deleteList(relocs,free);
}


void writeVarToData(VarInfo* var)
{
  //todo: store address within var?
  int symIdx=getSymtabIdx(newBinary,var->name,0);//todo: this will not
                                                 //work for
                                                 //local/static vars
  if(STN_UNDEF==symIdx)
  {death("could not get symbol for var %s\n",var->name);}
  GElf_Sym sym;
  getSymbol(newBinary,symIdx,&sym);
  Elf_Scn* scn=elf_getscn(newBinary->e,sym.st_shndx);
  GElf_Shdr shdr;
  if(!gelf_getshdr(scn,&shdr))
  {death("gelf_shdr failed\n");}
  byte* data=NULL;
  bool freeData=false;
  if(shdr.sh_type==SHT_PROGBITS)
  {
    data=getDataAtAbs(scn,sym.st_value,IN_MEM);
  }
  else if(shdr.sh_type==SHT_NOBITS)
  {
    data=zmalloc(var->type->length);
    freeData=true;
  }
  else
  {
    death("variable lives in unexpected section\n");
  }
  //todo: what if it's in rodata? Then shouldn't need to put it in .data.new too
  addr_t addr=addDataToScn(getDataByERS(patch,ERS_DATA),data,var->type->length);
  if(freeData)
  {
    free(data);
  }
  //now we must create a symbol for it as well
  ElfXX_Sym symNew=gelfSymToNativeSym(sym);
  symNew.st_name=addStrtabEntry(patch,var->name);
  symNew.st_value=addr;//will get relocated later once the data section in the patch has a fixed address
  symNew.st_shndx=elf_ndxscn(getSectionByERS(patch,ERS_DATA));
  addSymtabEntry(patch,getDataByERS(patch,ERS_SYMTAB),&symNew);
}

//takes a list of var transformations and actually constructs the ELF sections
void writeVarTransforms(List* varTrans)
{
  List* li=varTrans;
  for(;li;li=li->next)
  {
    VarTransformation* vt=li->value;
    //note: no longer writing the old type as we don't use it for anything currently
    //writeOldTypeToDwarf(vt->transform->from,vt->var->type->cu);
    writeVarToDwarf(dbg,vt->var,vt->cu,false);
    writeVarToData(vt->var);
    if(vt->transform) //will be no transformation if just changing the initializer
    {
      writeTransformationToDwarf(dbg,vt->transform);
      logprintf(ELL_INFO_V2,ELS_PATCHWRITE,"writing transformation for var %s\n",vt->var->name);
    }
    /*
    //create the symbol entry
    //this means first creating the string table entry
    int strIdx=addStrtabEntry(vt->var->name);
    ElfXX_Sym sym;
    sym.st_name=strIdx;
    sym.st_value=0;//don't know yet where this symbol is going to end up
    sym.st_size=vt->var->type->length;
    //todo: support local, weak, etc symbols
    sym.st_info=ELFXX_ST_INFO(STB_GLOBAL,STT_OBJECT);
    sym.st_other=0;
    sym.st_shndx=0;//no special section this is related to;
    addSymtabEntry(patch_syms_rel_data,&sym);*/
  }
}

void writeNewVarsForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  VarInfo** patchVars=(VarInfo**)dictValues(cuNew->tv->globalVars);
  Dictionary* oldVars=cuOld->tv->globalVars;
  VarInfo* var=patchVars[0];
  for(int i=0;var;i++,var=patchVars[i])
  {
    VarInfo* oldVar=dictGet(oldVars,var->name);
    if(!oldVar)
    {
      logprintf(ELL_INFO_V1,ELS_TYPEDIFF,"Found new variable %s\n",var->name);
      writeVarToDwarf(dbg,var,cuNew,true);
      writeVarToData(var);
    }
  }
  free(patchVars);
}

//returns a list of type VarTransformation
List* getTypeTransformationInfoForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  List* varTransHead=NULL;
  List* varTransTail=NULL;
  logprintf(ELL_INFO_V1,ELS_PATCHWRITE,"Examining compilation unit %s for type transformation info\n",cuOld->name);
  VarInfo** vars1=(VarInfo**)dictValues(cuOld->tv->globalVars);

  VarInfo* var=vars1[0];
  Dictionary* patchVars=cuNew->tv->globalVars;
  for(int i=0;var;i++,var=vars1[i])
  {
    VarInfo* patchedVar=dictGet(patchVars,var->name);
    if(!patchedVar)
    {
      //todo: do we need to do anything special to handle removal of variables in the patch?
      logprintf(ELL_WARN,ELS_PATCHWRITE,"var %s seems to have been removed in the patch\n",var->name);
      continue;
    }
    TypeInfo* ti1=var->type;
    TypeInfo* ti2=patchedVar->type;
    bool needsTransform=false;

    if(ti1->transformer)
    {
      needsTransform=true;
    }
    else
    {
      //todo: should have some sort of caching for types we've already
      //determined to be equal
      if(!compareTypesAndGenTransforms(ti1,ti2))
      {
        if(!ti1->transformer)
        {
          //todo: may not want to actually abort, may just want to issue
          //an error
          fprintf(stderr,"Error, cannot generate type transformation for variable %s\n",var->name);
          fflush(stderr);
          abort();
        }
        logprintf(ELL_INFO_V1,ELS_TYPEDIFF,"generated type transformation for type %s\n",ti1->name);
        needsTransform=true;
      }
      else
      {
        //todo: this is ugly separate out into its own function
        
        //what if the initializers for the variables have changed
        idx_t symIdxOld=getSymtabIdx(cuOld->elf,var->name,0);
        idx_t symIdxNew=getSymtabIdx(cuNew->elf,var->name,0);
        if(symIdxOld!=STN_UNDEF && symIdxNew!=STN_UNDEF)
        {       
          //check to see if its initializer has changed
          GElf_Sym symOld;
          GElf_Sym symNew;
          getSymbol(cuOld->elf,symIdxOld,&symOld);
          getSymbol(cuNew->elf,symIdxNew,&symNew);
          if(symOld.st_shndx!=SHN_COMMON && symOld.st_shndx!=SHN_UNDEF &&
             symNew.st_shndx!=SHN_COMMON && symNew.st_shndx!=SHN_UNDEF &&
             symNew.st_shndx!=SHN_ABS && symOld.st_shndx!=SHN_ABS)
          {
            GElf_Shdr shdrOld;
            GElf_Shdr shdrNew;
            Elf_Scn* scnOld=elf_getscn(cuOld->elf->e,symOld.st_shndx);
            Elf_Scn* scnNew=elf_getscn(cuNew->elf->e,symNew.st_shndx);
            getShdr(scnOld,&shdrOld);
            getShdr(scnNew,&shdrNew);
            if(shdrOld.sh_type!=SHT_NOBITS && shdrNew.sh_type!=SHT_NOBITS)
            {
              byte* initializerOld=getDataAtAbs(scnOld,
                                              symOld.st_value,IN_MEM);
              byte* initializerNew=getDataAtAbs(scnNew,
                                              symNew.st_value,IN_MEM);
              if(memcmp(initializerOld,initializerNew,ti1->length))
              {
                logprintf(ELL_INFO_V1,ELS_TYPEDIFF,"Initializer changed for var %s of type %s\n",var->name,ti1->name);
                if(TT_CONST==ti1->type)
                {
                  //we can copy over the data with impunity,
                  //the target shouldn't have changed it
                  needsTransform=true;
                }
                else
                {
                  logprintf(ELL_WARN,ELS_DWARFTYPES,"Variable %s (in %s) has a different initializer between versions. Doing nothing about this, but this may not be what you want\n",var->name,cuNew->name);
                }
              }
            }
            else if(shdrOld.sh_type!=shdrNew.sh_type)
            {
              if(TT_CONST==ti1->type)
              {
                //we can copy over the data with impunity,
                //the target shouldn't have changed it
                needsTransform=true;
              }
              else
              {
                logprintf(ELL_WARN,ELS_DWARFTYPES,"Variable %s (in %s) has a different initializer between versions. Doing nothing about this, but this may not be what you want\n",var->name,cuNew->name);
              }
            }
          }
          else if(symOld.st_shndx!=symNew.st_shndx)
          {
            death("This isn't implemented yet, poke James to implement it\n");
            //todo: what if just one is undefined/common?
          }
            //todo: handle SHN_ABS properly
        }
        else if((symIdxNew==STN_UNDEF && symIdxOld!=STN_UNDEF) ||
                (symIdxNew!=STN_UNDEF && symIdxOld==STN_UNDEF))
        {
          //todo: what do we actually want to do here
          logprintf(ELL_WARN,ELS_DWARFTYPES,"In one version of %s, variable %s does not have a symbol. Ignoring this, may not be what you want\n",cuNew->name,var->name);
        }
        
      }
    }
    if(needsTransform)
    {
      List* li=zmalloc(sizeof(List));
      VarTransformation* vt=zmalloc(sizeof(VarTransformation));
      vt->var=patchedVar;
      vt->cu=cuNew;

      //never write an FDE transformation for const global variables
      //because the program should never change them, should
      //be changed by initializer only
      if(TT_CONST!=vt->var->type->type)
      {
        vt->transform=var->type->transformer;
        if(!vt->transform)
        {
          death("the transformation info for that variable doesn't exist!\n");
        }

      }
      li->value=vt;
      if(varTransHead)
      {
        varTransTail->next=li;
      }
      else
      {
        varTransHead=li;
      }
      varTransTail=li;
    }
  }
  free(vars1);
  return varTransHead;
}

void addUnsafeSubprogram(SubprogramInfo* sub)
{
  idx_t symIdx=getSymtabIdx(sub->cu->elf,sub->name,0);
  idx_t symIdxInPatch=addSymbolFromBinaryToPatch(sub->cu->elf,symIdx);
  addDataToScn(getDataByERS(patch,ERS_UNSAFE_FUNCTIONS),&symIdxInPatch,sizeof(idx_t));
}

//returns the offset into the patch text that it was written at
addr_t writeFuncToPatchText(SubprogramInfo* func,CompilationUnit* cu,idx_t* outSymIdx)
{
  int len=func->highpc-func->lowpc;
  Elf_Scn* textScn=getSectionByERS(patch,ERS_TEXT);
  Elf_Data* textData=getDataByERS(patch,ERS_TEXT);
  GElf_Shdr shdr;
  gelf_getshdr(textScn,&shdr);
  
  Elf_Scn* textScnPatched=NULL;
  char buf[1024];
  snprintf(buf,1024,".text.%s",func->name);
  textScnPatched=getSectionByName(cu->elf,buf);
  if(!textScnPatched)
  {
    textScnPatched=getSectionByERS(cu->elf,ERS_TEXT);
  }
  assert(textScnPatched);
  addr_t offset=addDataToScn(textData,getDataAtAbs(textScnPatched,func->lowpc,IN_MEM),len);
  addr_t funcSegmentBase=offset+shdr.sh_addr;//where text for this function is based
  //if -ffunction-sections is used, the function might have its own text section
  ElfXX_Sym sym;
  sym.st_name=addStrtabEntry(patch,func->name);
  sym.st_value=offset;//is it ok that this is section-relative, since
  //we set st_shndx to be the text section?
  sym.st_size=0;
  sym.st_info=ELFXX_ST_INFO(STB_GLOBAL,STT_FUNC);
  sym.st_other=0;
  sym.st_shndx=elf_ndxscn(textScn);
  *outSymIdx=addSymtabEntry(patch,getDataByERS(patch,ERS_SYMTAB),&sym);
  

  Elf_Scn* relocScn=getRelocationSection(cu->elf,func->name);
  writeRelocationsInRange(func->lowpc,func->highpc,relocScn,funcSegmentBase,cu->elf);
  return offset;
}

void writeNewFuncsForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  SubprogramInfo** patchFuncs=(SubprogramInfo**)dictValues(cuNew->subprograms);
  Dictionary* oldFuncs=cuOld->subprograms;
  SubprogramInfo* func=patchFuncs[0];
  for(int i=0;func;i++,func=patchFuncs[i])
  {
    SubprogramInfo* oldFunc=dictGet(oldFuncs,func->name);
    if(!oldFunc)
    {
      logprintf(ELL_INFO_V1,ELS_TYPEDIFF,"Found new function %s\n",func->name);
      idx_t symIdx;
      addr_t offset=writeFuncToPatchText(func,cuNew,&symIdx);
      int len=func->highpc-func->lowpc;
      writeFuncToDwarf(dbg,func->name,offset,len,symIdx,cuNew,true);
    }
  }
  free(patchFuncs);
}

void writeFuncTransformationInfoForCU(CompilationUnit* cuOld,CompilationUnit* cuNew)
{
  SubprogramInfo** funcs1=(SubprogramInfo**)dictValues(cuOld->subprograms);
  for(int i=0;funcs1[i];i++)
  {
    SubprogramInfo* func=funcs1[i];
    SubprogramInfo* patchedFunc=dictGet(cuNew->subprograms,func->name);
    if(!patchedFunc)
    {
      logprintf(ELL_WARN,ELS_PATCHWRITE,"function %s was removed from the patched version of compilation unit %s\n",func->name,cuNew->name);
      continue;
    }
    if(!areSubprogramsIdentical(func,patchedFunc,cuOld->elf,cuNew->elf))
    {
      logprintf(ELL_INFO_V2,ELS_PATCHWRITE,"writing transformation info for function %s\n",func->name);
      //we also have to add an entry into debug info, so that
      //we know to patch the function
      idx_t symIdx;
      addr_t offset=writeFuncToPatchText(patchedFunc,cuNew,&symIdx);
      int len=patchedFunc->highpc-patchedFunc->lowpc;
      writeFuncToDwarf(dbg,func->name,offset,len,symIdx,cuNew,false);
      addUnsafeSubprogram(patchedFunc);
    }
    else if(func->hasVariableParams)
    {
      logprintf(ELL_INFO_V2,ELS_SAFETY,"Function %s has variable parameters. Erring on the side of caution and adding it to unsafe list\n",patchedFunc->name);
      addUnsafeSubprogram(patchedFunc);
    }
    else
    {
      //even if the subprogram hasn't changed, it might possibly
      //not be safe to patch while it's on the stack based on the types it uses
      //go through and see if they're unsafe

      //todo: this is not a sufficient determination. Also need to look at relocations
      //for changed global variables, because that stuff isn't necessarily going to
      //show up in the DWARF information
      
      logprintf(ELL_INFO_V2,ELS_DWARFTYPES,"Examining types used in subprogram %s\n",patchedFunc->name);
      List* li=patchedFunc->typesHead;
      for(;li;li=li->next)
      {
        TypeInfo* type=li->value;
        logprintf(ELL_INFO_V3,ELS_DWARFTYPES,"Subprogram %s uses type %s\n",patchedFunc->name,type->name);
        if(type->transformer)
        {
          logprintf(ELL_INFO_V2,ELS_SAFETY,"type %s needs transform, marking subprogram %s as unsafe\n",type->name,patchedFunc->name);
          addUnsafeSubprogram(patchedFunc);
          break;
        }
      }
    }
  }


  free(funcs1);
}

  
void writeTypeAndFuncTransformationInfo(ElfInfo* patchee,ElfInfo* patched)
{
  DwarfInfo* diPatchee=patchee->dwarfInfo;
  DwarfInfo* diPatched=patched->dwarfInfo;
  //  List* varTransHead=NULL;
  //  List* varTransTail=NULL;
  
      //todo: handle addition of variables and also handle
  //      things moving between compilation units,
  //      perhaps group global objects from all compilation units
  //      together before dealing with them.
  List* cuLi1=diPatchee->compilationUnits;
  for(;cuLi1;cuLi1=cuLi1->next)
  {
    CompilationUnit* cuOld=cuLi1->value;
    CompilationUnit* cuNew=NULL;
    //find the corresponding compilation unit in the patched process
    //note: we assume that the number of compilation units is not so large
    //that using a hash table would give us much better performance than
    //just going through a list. If working on a truly enormous project it
    //might make sense to do this
    List* cuLi2=diPatched->compilationUnits;
    for(;cuLi2;cuLi2=cuLi2->next)
    {
      cuNew=cuLi2->value;
      if(cuOld->name && cuNew->name && !strcmp(cuOld->name,cuNew->name))
      {
        break;
      }
      cuNew=NULL;
    }
    if(!cuNew)
    {
      fprintf(stderr,"WARNING: the patched version omits an entire compilation unit present in the original version.\n");
      if(cuOld->name)
      {
        fprintf(stderr,"Missing cu is named \"%s\"\n",cuOld->name);
      }
      else
      {
        fprintf(stderr,"The compilation unit in the patchee version does not have a name\n");
      }
      //todo: we also need to handle a compilation unit being present
      //in the patched version and not the patchee
      break;
    }
    cuOld->presentInOtherVersion=true;
    cuNew->presentInOtherVersion=true;
    List* varTransformsList=getTypeTransformationInfoForCU(cuOld,cuNew);
    //go through the variables that have changed and find
    //all functions that use those variables and mark them for possible unsafety
    for(List* li=varTransformsList;li;li=li->next)
    {
      VarInfo* var=((VarTransformation*)li->value)->var;
      idx_t symIdx=getSymtabIdx(cuNew->elf,var->name,0);
      if(STN_UNDEF==symIdx)
      {
        death("Could not find symbol for variable %s\n",var->name);
      }
      List* relocations=getRelocationItemsFor(cuNew->elf,symIdx);
      for(List* liR=relocations;liR;liR=liR->next)
      {
        RelocInfo* reloc=liR->value;
        GElf_Shdr shdr;
        if(!gelf_getshdr(elf_getscn(reloc->e->e,reloc->scnIdx),&shdr))
        {
          death("gelf_getshdr failed\n");
        }
        char* scnName=getScnHdrString(reloc->e,shdr.sh_name);
        if(!strncmp(".text",scnName,5))
        {
          idx_t funcSymIdx=findSymbolContainingAddress(cuNew->elf,reloc->r_offset,STT_FUNC,reloc->scnIdx);
          if(STN_UNDEF==funcSymIdx)
          {
            printf("reloc in section %i\n",reloc->scnIdx);
            logprintf(ELL_WARN,ELS_PATCHWRITE,"Could not find any function containing one of the relocations for variable %s\n",var->name);
            continue;
          }
          GElf_Sym funcSym;
          getSymbol(cuNew->elf,funcSymIdx,&funcSym);
          char* funcName=getString(cuNew->elf,funcSym.st_name);
          SubprogramInfo* subprogram=dictGet(cuNew->subprograms,funcName);
          if(!subprogram)
          {
            death("Function %s expected in compilation unit %s but its info structure was not found\n",funcName,cuNew->name);
          }
          //todo: if the SubprogramInfo struct had a flag bool unsafe
          //or something like that, we could just set that
          //since we actually know this type will make things unsafe
          List* typeLi=zmalloc(sizeof(List));
          typeLi->value=var->type;
          listAppend(&subprogram->typesHead,&subprogram->typesTail,typeLi);
          logprintf(ELL_INFO_V2,ELS_SAFETY,"Added type %s to types used by function %s which would make it unsafe\n",var->type->name,subprogram->name);
        }
      }
      deleteList(relocations,free);
    }
    writeNewVarsForCU(cuOld,cuNew);
    writeVarTransforms(varTransformsList);
    deleteList(varTransformsList,free);

    writeNewFuncsForCU(cuOld,cuNew);
    //note that this must be done after dealing with the data
    //so that dealing with the data writes out the appropriate symbols
    writeFuncTransformationInfoForCU(cuOld,cuNew);


    logprintf(ELL_INFO_V2,ELS_PATCHWRITE,"completed all transformations for compilation unit %s\n",cuOld->name);

  }
}

//takes an elf object that only has a pached version, no patchee version
//and writes it all out
void writeAllTypeAndFuncTransformationInfo(ElfInfo* elf)
{
  death("This function isn't written yet b/c James is an idiot\n");
}




//todo: in the future only copy rodata from object files which changed,
//don't necessarily need *all* the rodata
void writeROData(ElfInfo* binary)
{
  if(hasERS(binary,ERS_RODATA))
  {
    Elf_Data* roData=getDataByERS(binary,ERS_RODATA);
    Elf_Data* roDataPatch=getDataByERS(patch,ERS_RODATA);
    uint len=roData->d_size;
    rodataOffset=roDataPatch->d_size;//offset to put things off of now
    addDataToScn(roDataPatch,roData->d_buf,len);
  }
  //sometimes objects have no rodata. Not terribly common, but it happens
}


ElfInfo* createPatch(char* oldSourceTree,char* newSourceTree,char* oldBinName,char* newBinName,FILE* patchOutfile,char* filename)
{
  if(!patchOutfile)
  {
    patchOutfile=fopen(filename,"w");
    if(!patchOutfile)
    {
      death("Could not create patch because could not open file %s for writing\n",filename);
    }
  }
  newBinary=openELFFile(newBinName);
  if(!newBinary)
  {
    death("Could not create patch because could not open new binary\n");
  }
  findELFSections(newBinary);
  oldBinary=openELFFile(oldBinName);
  if(!oldBinary)
  {
    death("Could not create patch because could not open old binary\n");
  }
  findELFSections(oldBinary);
  patch=startPatchElf(patchOutfile,filename);
  Dwarf_Error err;
  //todo: always creating 32-bit Dwarf sections. DWARF standard
  //recommends never having 64-bit Dwarf as the default, even on
  //x86_64. We should have some way of recognizing when we want 64-bit
  //though, if things are really huge.
  dbg=dwarf_producer_init(DW_DLC_WRITE|DW_DLC_SIZE_32,dwarfWriteSectionCallback,dwarfErrorHandler,NULL,&err);
  Dwarf_P_Die tmpRootDie=dwarf_new_die(dbg,DW_TAG_compile_unit,NULL,NULL,NULL,NULL,&err);
  dwarf_add_die_to_debug(dbg,tmpRootDie,&err);
  cie=dwarf_add_frame_cie(dbg,"",1,1,0,NULL,0,&err);
  if(DW_DLV_NOCOUNT==cie)
  {
    dwarfErrorHandler(err,"creating frame cie failed");
  }

  //todo: find better way of reording symbols so local ones all first
  //call this once so the necessary dwarf sections get created
  //dwarf_transform_to_disk_form(dbg,&err);

  //so far there should be only local sections in symtab. Can set sh_info
  //apropriately
  ElfXX_Shdr* shdr=elfxx_getshdr(getSectionByERS(patch,ERS_SYMTAB));
  shdr->sh_info=getDataByERS(patch,ERS_SYMTAB)->d_off/sizeof(ElfXX_Sym)+1;
  
  //now that we've created the necessary things, actually run through
  //the stuff to write in our data
  List* objFiles=getChangedObjectFilesInSourceTree(oldSourceTree,newSourceTree);
  List* li=objFiles;
  for(;li;li=li->next)
  {
    ObjFileInfo* obj=li->value;
    switch(obj->state)
    {
    case EOS_MODIFIED:
      {
        ElfInfo* elf1=getOriginalObject(obj);
        ElfInfo* elf2=getModifiedObject(obj);
        logprintf(ELL_INFO_V1,ELS_PATCHWRITE,"Finding differences between %s and %s and writing them to the patch\n",elf1->fname,elf2->fname);
        readDWARFTypes(elf1,oldSourceTree);
        readDWARFTypes(elf2,newSourceTree);
        if(!elf1->dwarfInfo && !elf2->dwarfInfo)
        {
          logprintf(ELL_WARN,ELS_PATCHWRITE,"Assuming that because %s and %s don't have Dwarf information, they will not need patching. If this assumption is incorrect, please fix your compilation process so they do contain DWARF information\n",elf1->fname,elf2->fname);
        }
        else if(!elf1->dwarfInfo || !elf2->dwarfInfo)
        {
          death("One of %s and %s has DWARF information and the other does not. This is unexpected\n",elf1->fname,elf2->fname);
        }
        else
        {
          //we actually got DWARF data!
          
          //all the object files had their own roData sections
          //and now we're lumping them together
          writeROData(elf2);
          writeTypeAndFuncTransformationInfo(elf1,elf2);
        }
        endELF(elf1);
        endELF(elf2);
      }
    break;
    case EOS_NEW:
      {
        ElfInfo* elf=getModifiedObject(obj);
        readDWARFTypes(elf,newSourceTree);
        writeAllTypeAndFuncTransformationInfo(elf);
      }
    default:
      death("should only be seeing changed object files\n");
    }
  }
  deleteList(objFiles,(FreeFunc)deleteObjFileInfo);

  dwarf_add_die_to_debug(dbg,firstCUDie,&err);
  int numSections=dwarf_transform_to_disk_form(dbg,&err);
  for(int i=0;i<numSections;i++)
  {
    Dwarf_Signed elfScnIdx;
    Dwarf_Unsigned length;
    Dwarf_Ptr buf=dwarf_get_section_bytes(dbg,i,&elfScnIdx,&length,&err);
    Elf_Scn* scn=elf_getscn(patch->e,elfScnIdx);
    Elf_Data* data=elf_newdata(scn);
    ElfXX_Shdr* shdr=elfxx_getshdr(scn);
    shdr->sh_size=length;
    data->d_size=length;
    data->d_off=0;
    data->d_buf=zmalloc(length);
    memcpy(data->d_buf,buf,length);
  }
  
  finalizeModifiedElf(patch);
  int res=dwarf_producer_finish(dbg,&err);
  if(DW_DLV_ERROR==res)
  {
    dwarfErrorHandler(err,NULL);
  }
  endELF(oldBinary);
  endELF(newBinary);

  if(elf_update (patch->e, ELF_C_WRITE) <0)
  {
    death("Failed to write out elf file %s: %s\n",patch->fname,elf_errmsg (-1));
    exit(1);
  }
  return patch;
}

