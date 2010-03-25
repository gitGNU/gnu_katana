/*
  File: elfcmp.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: Compares two elf files for equality. The idea and name come from Red Hat Elfutils but the code is not in any way based upon that code.
*/

#include <libelf.h>
#include <stdbool.h>

//compares the elf files found at two filepaths
//returns false if they are not identical
bool elfcmp(char* path1,char* path2)
{
  //todo: implement this function
  return false;
}
