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
  int dataStart;
  Elf_Data* textData;
  int textStart;
  int fd;//file descriptor for elf file
} ElfInfo;


ElfInfo* openELFFile(char* fname);
void endELF(ElfInfo* _e);
List* getRelocationItemsFor(ElfInfo* e,int symIdx);
word_t getTextAtRelOffset(ElfInfo* e,int offset);
addr_t getSymAddress(ElfInfo* e,int symIdx);
//return NULL if symbol with the given index doesn't exist
bool getSymbol(ElfInfo* e,int symIdx,GElf_Sym* outSym);
int getSymtabIdx(ElfInfo* e,char* symbolName);
void printSymTab(ElfInfo* e);
void writeOut(ElfInfo* e,char* outfname);
void findELFSections(ElfInfo* e);
#endif
