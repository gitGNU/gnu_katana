/*
  File: patchwrite.c
  Author: James Oakley
  Project: Katana
  Date: January 2010
  Description: Write patch information out to a po (patch object) file
*/

#include "types.h"
#include "dwarftypes.h"
#include <gelf.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct
{
  VarInfo* var;
  TypeTransform* transform;
} VarTransformation;

Elf_Data* patch_syms_rel_data=NULL;
Elf_Data* patch_syms_new_data=NULL;
Elf_Data* patch_rules_data=NULL;
Elf_Data* patch_expr_data=NULL;
Elf_Data* strtab_data=NULL;

void addStrtabEntry(char* str)
{
  int len=strlen(str)+1;
  //we have co-opted the d_off field of Elf_Data to determine
  //where in the d_buf to start writing. It will be zeroed before
  //the elf file is actually written. We allocate larger memory
  //for d_buf as needed
  if(len>strtab_data->d_size-strtab_data->d_off)
  {
    //ran out of space, allocate more
    strtab_data->d_size*=2;
    strtab_data->d_buf=realloc(strtab_data->d_buf,strtab_data->d_size);
  }
  memcpy((char*)strtab_data->d_buf+strtab_data->d_off,str,len);
    strtab_data->d_off+=len;

}

//takes a list of var transformations and actually constructs the ELF sections
void writeVarTransforms(List* varTrans)
{
  List* li=varTrans;
  for(;li;li=li->next)
  {
    VarTransformation* vt=li->value;
    printf("writing transformation for var %s\n",vt->var->name);
    //create the symbol entry
    //this means first creating the string table entry
    addStrtabEntry(vt->var->name);
  }
}

void writeTypeTransformationInfo(DwarfInfo* diPatchee,DwarfInfo* diPatched)
{
  List* varTransHead=NULL;
  List* varTransTail=NULL;
  
      //todo: handle addition of variables and also handle
  //      things moving between compilation units,
  //      perhaps group global objects from all compilation units
  //      together before dealing with them.
  List* cuLi1=diPatchee->compilationUnits;
  for(;cuLi1;cuLi1=cuLi1->next)
  {
    CompilationUnit* cu1=cuLi1->value;
    CompilationUnit* cu2=NULL;
    //find the corresponding compilation unit in the patched process
    //note: we assume that the number of compilation units is not so large
    //that using a hash table would give us much better performance than
    //just going through a list. If working on a truly enormous project it
    //might make sense to do this
    List* cuLi2=diPatched->compilationUnits;
    for(;cuLi2;cuLi2=cuLi2->next)
    {
      cu2=cuLi2->value;
      if(cu1->name && cu2->name && !strcmp(cu1->name,cu2->name))
      {
        break;
      }
      cu2=NULL;
    }
    if(!cu2)
    {
      fprintf(stderr,"WARNING: the patched version omits an entire compilation unit present in the original version.\n");
      if(cu1->name)
      {
        fprintf(stderr,"Missing cu is named %s\n",cu1->name);
      }
      else
      {
        fprintf(stderr,"The compilation unit in the patchee version does not have a name\n");
      }
      //todo: we also need to handle a compilation unit being present
      //in the patched version and not the patchee
      break;
    }
    cu1->presentInOtherVersion=true;
    cu2->presentInOtherVersion=true;
    
    TransformationInfo* trans=zmalloc(sizeof(TransformationInfo));
    trans->typeTransformers=dictCreate(100);//todo: get rid of magic # 100
    printf("Examining compilation unit %s\n",cu1->name);
    VarInfo** vars1=(VarInfo**)dictValues(cu1->tv->globalVars);
    //todo: handle addition of variables in the patch
    VarInfo* var=vars1[0];
    Dictionary* patchVars=cu2->tv->globalVars;
    for(int i=0;var;i++,var=vars1[i])
    {
      printf("Found variable %s \n",var->name);
      VarInfo* patchedVar=dictGet(patchVars,var->name);
      if(!patchedVar)
      {
        //todo: do we need to do anything special to handle removal of variables in the patch?
        printf("warning: var %s seems to have been removed in the patch\n",var->name);
        continue;
      }
      TypeInfo* ti1=var->type;
      TypeInfo* ti2=patchedVar->type;
      bool needsTransform=false;
      if(dictExists(trans->typeTransformers,ti1->name))
      {
        needsTransform=true;
      }
      else
      {
        //todo: should have some sort of caching for types we've already
        //determined to be equal
        TypeTransform* transform=NULL;
        if(!compareTypes(ti1,ti2,&transform))
        {
          if(!transform)
          {
            //todo: may not want to actually abort, may just want to issue
            //an error
            fprintf(stderr,"Error, cannot generate type transformation for variable %s\n",var->name);
            fflush(stderr);
            abort();
          }
          printf("generated type transformation for type %s\n",ti1->name);
          dictInsert(trans->typeTransformers,ti1->name,transform);
          needsTransform=true;
        }
      }
      if(needsTransform)
      {
        List* li=zmalloc(sizeof(List));
        VarTransformation* vt=zmalloc(sizeof(VarTransformation));
        vt->var=var;
        vt->transform=(TypeTransform*)dictGet(trans->typeTransformers,var->type->name);
        if(!vt->transform)
        {
          death("the transformation info for that variable doesn't exist!\n");
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

    printf("completed all transformations for compilation unit %s\n",cu1->name);
    freeTransformationInfo(trans);
    free(vars1);
  }
  writeVarTransforms(varTransHead);
}

void writePatch(DwarfInfo* diPatchee,DwarfInfo* diPatched,char* fname)
{
  int outfd = creat(fname, 0666);
  if (outfd < 0)
  {
    fprintf(stderr,"cannot open output file '%s'", fname);
  }
  Elf *outelf = elf_begin (outfd, ELF_C_WRITE, NULL);
  Elf32_Ehdr* ehdr=elf32_newehdr(outelf);
  if(!ehdr)
  {
    death("Unable to create new ehdr\n");
  }
  ehdr->e_ident[EI_MAG0]=ELFMAG0;
  ehdr->e_ident[EI_MAG1]=ELFMAG1;
  ehdr->e_ident[EI_MAG2]=ELFMAG2;
  ehdr->e_ident[EI_MAG3]=ELFMAG3;
  ehdr->e_ident[EI_CLASS]=ELFCLASS32;
  ehdr->e_ident[EI_DATA]=ELFDATA2LSB;
  ehdr->e_ident[EI_VERSION]=EV_CURRENT;
  ehdr->e_ident[EI_OSABI]=ELFOSABI_NONE;
  ehdr->e_machine=EM_NONE;
  ehdr->e_type=ET_NONE;//not relocatable, or executable, or shared object, or core, etc
  ehdr->e_version=EV_CURRENT;

  //first create the string table
  Elf_Scn* strtab_scn=elf_newscn(outelf);
  strtab_data=elf_newdata(strtab_scn);
  strtab_data->d_align=1;
  strtab_data->d_buf=malloc(8);//arbitrary starting size, more will be allocced as needed
  strtab_data->d_off=0;
  strtab_data->d_size=8;//again, will increase as needed
  strtab_data->d_version=EV_CURRENT;
  
  //GElf_Shdr* shdr=NULL;
  //shdr=gelf_getshdr(strtab_scn, shdr);
  Elf32_Shdr * shdr ;
  shdr=elf32_getshdr(strtab_scn);
  shdr->sh_type=SHT_STRTAB;
  shdr->sh_link=SHN_UNDEF;
  shdr->sh_info=SHN_UNDEF;
  shdr->sh_addralign=1;
  shdr->sh_name=0;//first entry in the string table
  ehdr->e_shstrndx=elf_ndxscn(strtab_scn);//set strtab in elf header


  
  
  addStrtabEntry(".strtab");
  //now that we've created the necessary things, actually run through
  //the stuff to write in our data
  writeTypeTransformationInfo(diPatchee,diPatched);

  //our functions for dealing with dynamically increasing the size of the data
  //sections coopted some of the fields for their own use. Now put them back
  //to what they should be
  strtab_data->d_size=strtab_data->d_off;//what was actually used
  strtab_data->d_off=0;
  strtab_data->d_buf=realloc(strtab_data->d_buf,strtab_data->d_size);
  shdr=elf32_getshdr(strtab_scn);
  shdr->sh_size=strtab_data->d_size;

  //now I don't remember what this was for and it seems to cause problems
  //elf_flagelf (outelf, ELF_C_SET, ELF_F_LAYOUT);


  if (elf_update (outelf, ELF_C_WRITE) <0)
  {
    fprintf(stdout,"Failed to write out elf file: %s\n",elf_errmsg (-1));
  }

  elf_end (outelf);
  close (outfd);
  printf("wrote elf file\n");
}

