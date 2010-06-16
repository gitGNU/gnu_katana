/*
  File: fdedump.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: February 2010
  Description: Procedures to print out information about the fdes contained in an ELF file,
               specifically geared towards printing out FDEs in Patch Objects with
               Katana's special registers and operations
*/

#include "elfparse.h"
#include "fderead.h"
void printCIEInfo(CIE* cie);
void printPatchFDEInfo(ElfInfo* patch);
