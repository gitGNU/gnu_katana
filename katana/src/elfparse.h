/*
  File: elfparse.h
  Author: James Oakley
  Project: Katana
  Date: January 10
  Description: Read information from an ELF file
*/

#ifndef elfparse_h
#define elfparse_h
#include <libelf.h>
#include <gelf.h>
#include "util/util.h"
#include "types.h"
#include <elf.h>

typedef enum
{
  IN_MEM=1,
  ON_DISK=2
} ELF_STORAGE_TYPE;

typedef enum
{
  ERS_TEXT=1,
  ERS_RODATA,
  ERS_DATA,
  ERS_SYMTAB,
  ERS_STRTAB,
  ERS_HASHTABLE,
  ERS_RELA_TEXT,
  ERS_PLT,
  ERS_REL_PLT,
  ERS_DYNSYM,
  ERS_DYNSTR,
  ERS_DYNAMIC,
  ERS_UNSAFE_FUNCTIONS,
  ERS_DEBUG_INFO,
  ERS_CNT,
  ERS_INVALID
} E_RECOGNIZED_SECTION;

typedef struct ElfInfo
{
  int symTabCount;
  Elf* e;
  size_t sectionHdrStrTblIdx;
  size_t strTblIdx;
  Elf_Data* textRelocData;
  int textRelocCount;
  int sectionIndices[ERS_CNT];
  int dataStart[2];//in memory and on disk
  int textStart[2];//in memory and on disk
  int fd;//file descriptor for elf file
  char* fname;//file name associated with descriptor
  DwarfInfo* dwarfInfo;
  struct FDE* fdes;//for relocatable and executable objects, these
                   //will be sorted by lowpc
  int numFdes;
  struct CIE* cie;
  bool dataAllocatedByKatana;//used for memory management
  bool isPO;//is this elf object a patch object?
} ElfInfo;


ElfInfo* openELFFile(char* fname);
void endELF(ElfInfo* _e);
void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
void setWordAtAbs(Elf_Scn* scn,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
word_t getWordAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getDataAtAbs(Elf_Scn* scn,addr_t addr,ELF_STORAGE_TYPE type);
void* getTextDataAtRelOffset(ElfInfo* e,int offset);
word_t getTextAtRelOffset(ElfInfo* e,int offset);
void printSymTab(ElfInfo* e);
//have to pass the name that the elf file will originally get written
//out to, because of the way elf_begin is set up
//if flushToDisk is false doesn't
//actually write to disk right now
ElfInfo* duplicateElf(ElfInfo* e,char* outfname,bool flushToDisk,bool keepLayout);
void writeOut(ElfInfo* e,char* outfname,bool keepLayout);
void findELFSections(ElfInfo* e);
Elf_Scn* getSectionByName(ElfInfo* e,char* name);
Elf_Data* getDataByIdx(ElfInfo* e,idx_t idx);
Elf_Data* getDataByERS(ElfInfo* e,E_RECOGNIZED_SECTION scn);
Elf_Scn* getSectionByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers);
void getShdrByERS(ElfInfo* e,E_RECOGNIZED_SECTION ers,GElf_Shdr* shdr);
void getShdr(Elf_Scn* scn,GElf_Shdr* shdr);
char* getSectionNameFromIdx(ElfInfo* e,int idx);
char* getScnHdrString(ElfInfo* e,int idx);
char* getString(ElfInfo* e,int idx);//get a string from the normal string table
char* getDynString(ElfInfo* e,int idx);//get a string from the dynamic string table



#define SHT_KATANA_UNSAFE_FUNCTIONS SHT_LOUSER+0x1
#endif
