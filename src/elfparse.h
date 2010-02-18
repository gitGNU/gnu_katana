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

typedef struct
{
  Elf_Data* hashTableData;
  Elf_Data* symTabData;
  int symTabCount;
  Elf* e;
  size_t sectionHdrStrTblIdx;
  size_t strTblIdx;
  Elf_Data* textRelocData;
  int textRelocCount;
  Elf_Data* dataData;//the data section
  int dataScnIdx;
  int dataStart[2];//in memory and on disk
  int textScnIdx;
  Elf_Data* textData;
  int textStart[2];//in memory and on disk
  Elf_Data* roData;
  int fd;//file descriptor for elf file
  char* fname;//file name associated with descriptor
  DwarfInfo* dwarfInfo;
  struct FDE* fdes;
  int numFdes;
} ElfInfo;


ElfInfo* openELFFile(char* fname);
void endELF(ElfInfo* _e);
void* getTextDataAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
word_t getTextAtAbs(ElfInfo* e,addr_t addr,ELF_STORAGE_TYPE type);
void setTextAtAbs(ElfInfo* e,addr_t addr,word_t value,ELF_STORAGE_TYPE type);
void* getTextDataAtRelOffset(ElfInfo* e,int offset);
word_t getTextAtRelOffset(ElfInfo* e,int offset);
int getSymtabIdx(ElfInfo* e,char* symbolName);
void printSymTab(ElfInfo* e);
//have to pass the name that the elf file will originally get written
//out to, because of the way elf_begin is set up
//if flushToDisk is false doesn't
//actually write to disk right now
ElfInfo* duplicateElf(ElfInfo* e,char* outfname,bool flushToDisk,bool keepLayout);
void writeOut(ElfInfo* e,char* outfname,bool keepLayout);
void findELFSections(ElfInfo* e);
Elf_Scn* getSectionByName(ElfInfo* e,char* name);
char* getSectionNameFromIdx(ElfInfo* e,int idx);
char* getScnHdrString(ElfInfo* e,int idx);
char* getString(ElfInfo* e,int idx);
#endif
