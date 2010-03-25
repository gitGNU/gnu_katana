/*
  File: elfcmp.h
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: Compares two elf files for equality. The idea and name come from Red Hat Elfutils but the code is not in any way based upon that code.
*/

#ifndef elfcmp_h
#define elfcmp_h
//compares the elf files found at two filepaths
bool elfcmp(char* path1,char* path2);
#endif
