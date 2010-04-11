/*
  File: linkmap.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: This file contains methods to facilitate finding functions that have been
               dynamically linked into the running target. It does this by examining the
               linkmaps of the target directly
  Attribution: this code is inspired by a linkmap method
               posted by the grugq to bugtraq in the post "More ELF buggery"
               http://seclists.org/bugtraq/2002/May/295
               The same technique was also known to Julien Vanegue,
               Author of Eresi. It is likely that this method may be part
               of Eresi's linkmap code although I have not dug into Eresi's
               linkmap code to find out the precise method used there.
 Important Note: the hash table in ELF files has entries of size Elf32_Word
                 regardless of the Elf class of the object
*/

#include <link.h>
#include "elfparse.h"
#include "target.h"
#include "util/logging.h"

addr_t locateLinkMap(ElfInfo* e)
{
  //go through all the dynamic entries until we find PLTGOT
  //which is where the GOT is stored
  //note: this is platform/OS specific, not in ELF standard
  Elf_Data* data=getDataByERS(e,ERS_DYNAMIC);
  int numEntries=data->d_size/sizeof(ElfXX_Dyn);
  addr_t gotAddr=0;
  for(int i=0;i<numEntries;i++)
  {
    GElf_Dyn dyn;
    if(!gelf_getdyn(data,i,&dyn))
    {
      death("gelf_getdyn failed\n");
    }
    if(dyn.d_tag==DT_PLTGOT)
    {
      gotAddr=dyn.d_un.d_ptr;
    }
  }
  //todo: make sure it's always got[1] where this is stored,
  //this may be architecture-specific

  //ok, so we have the address of got. got[1] stores the address
  //of the linkmap. Get it.
  addr_t result;
  memcpyFromTarget((byte*)&result,gotAddr+sizeof(addr_t),sizeof(addr_t));
  return result;
}

//looks for the dynamic symbol with a given name
//in a linkmap entry
//returns true (and stores result) on success
bool locateSymbolInLinkMap(struct link_map* lm,addr_t* result,char* symName,int symNameHash)
{
  //todo: this is unsafe, need to check pages mapped into target
  //to make sure this access will be ok
  char nameBuf[256];
  memcpyFromTarget((byte*)nameBuf,(addr_t)lm->l_name,256);
  nameBuf[255]=0;
  if(!strlen(nameBuf))
  {
    logprintf(ELL_WARN,ELS_LINKMAP,"Not examining symbols in nameless library, it's probably not what we want\n");
    return false;
  }
  
  logprintf(ELL_INFO_V2,ELS_LINKMAP,"Looking for symbol %s in link map for object %s\n",symName,nameBuf);
    
  addr_t strtab=0;
  addr_t symtab=0;
  addr_t hashtableBuckets=0;
  ElfXX_Word numBuckets=0;
  addr_t hashtableChains=0;
  addr_t hashtable=0;
  
  //first we look at the link the linkmap entry has to the .dynamic section
  //for whatever program or library it corresponds to
  ElfXX_Dyn dyn;
  for(int i=0;;i++)
  {
    memcpyFromTarget((byte*)&dyn,(addr_t)(lm->l_ld+i),sizeof(dyn));
    if(dyn.d_tag==DT_NULL)
    {
      break;//end of .dynamic
    }
    logprintf(ELL_INFO_V4,ELS_LINKMAP,"tag is %i\n",dyn.d_tag);
    switch(dyn.d_tag)
    {
    case DT_HASH:
      {
        hashtable=dyn.d_un.d_ptr;
      }
      break;
    case DT_STRTAB:
      strtab=dyn.d_un.d_ptr;
      break;
    case DT_SYMTAB:
      symtab=dyn.d_un.d_ptr;
    case DT_SONAME:
      break;
    default:
      break;
    }
  }

  if(!symtab || !strtab || !hashtable)
  {
    logprintf(ELL_WARN,ELS_LINKMAP,"Not examining symbols in library %s because not all the needed entries were found in the .dynamic section\n",nameBuf);
    return false;
  }

  //in practice, I sometimes see invalid hashtable entries and no good way that
  //I've found to detect them. So we just look for an invalid memory access and
  //bail when we get it, hope the symbol wasn't in that library
  if(!memcpyFromTargetNoDeath((byte*)&numBuckets,hashtable,sizeof(ElfXX_Word)))
  {
    logprintf(ELL_WARN,ELS_LINKMAP,"Not examining symbols in this library ('%s'), doesn't seem to contain valid hashtable\n",nameBuf);
    return false;
  }
  hashtableBuckets=hashtable+2*sizeof(ElfXX_Word);
  hashtableChains=hashtable+2*sizeof(ElfXX_Word)+numBuckets*sizeof(ElfXX_Word);

  //now that we've found what we need to look at the hashtable, we actually index into
  //the hash table
  ElfXX_Word symIdx=0;
  memcpyFromTarget((byte*)&symIdx,hashtableBuckets+sizeof(ElfXX_Word)*(symNameHash % numBuckets),sizeof(ElfXX_Word));
  for(;
      symIdx;
      memcpyFromTarget((byte*)&symIdx,hashtableChains+sizeof(idx_t)*symIdx,sizeof(ElfXX_Word)))
  {
    ElfXX_Sym sym;
    memcpyFromTarget((byte*)&sym,symtab+sizeof(ElfXX_Sym)*symIdx,sizeof(ElfXX_Sym));

    if(0==sym.st_value && SHN_UNDEF==sym.st_shndx)
    {
      //this is an import symbol
      return false;
    }
    
    //todo: using strnmatchTarget we don't support symbols with names
    //that are a substring of another symbol's name
    if(strnmatchTarget(symName,strtab+sym.st_name))
    {
      logprintf(ELL_INFO_V1,ELS_LINKMAP,"Found symbol %s in %s\n",symName,nameBuf);
      *result=lm->l_addr+sym.st_value;//l_addr is used to rebase the symbol index
      return true;
    }
  }
  return false;  
}

//the passed ElfInfo object must correspond to the
//currently running target known to the methods
//in target.c
addr_t locateRuntimeSymbolInTarget(ElfInfo* e,char* name)
{
  //todo: cache this
  addr_t linkmapAddr=locateLinkMap(e);
  //there is a linkmap entry for the original binary and for each library that's been linked
  //in. We scan all the link maps and look for the symbol in the hash table of each
  //todo: is there a global hash table or something we can use. Some comments in
  //the code by grugq (mentioned in Attribution in the file header) seem to indicate
  //that he considers this method slow

  struct link_map lm;
  memcpyFromTarget((byte*)&lm,linkmapAddr,sizeof(lm));
  for(;;memcpyFromTarget((byte*)&lm,(addr_t)lm.l_next,sizeof(lm)))
  {
    addr_t addr;//store the result address
    //can't seem to get rid of a sign cast warning in below line, it seems
    //different library versions of libelf have different signdness for the param there
    if(locateSymbolInLinkMap(&lm,&addr,name,elf_hash((const unsigned char*)name)))
    {
      return addr;
    }
    if(0==lm.l_next)
    {
      break;
    }
  }
  death("could not locate runtime symbol %s\n",name);
  return 0;
}
