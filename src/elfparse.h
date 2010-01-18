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
#include "util.h"
#include "types.h"

Elf* openELFFile(char* fname);
void endELF(Elf* _e);
List* getRelocationItemsFor(int symIdx);
word_t getTextAtRelOffset(int offset);
addr_t getSymAddress(int symIdx);
int getSymtabIdx(char* symbolName);
void printSymTab();
void writeOut(char* outfname);
void findELFSections();
#endif
