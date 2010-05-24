/*
  File: pmap.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: April 2010
  Description: Find out information about the target processes memory map.
               Requires a linux-like /proc filesystem. It should be useable
               on BSD and Solaris as well as Linux, but these have not been tested.
               FreeBSD seems to be getting rid of /proc

*/

#include <limits.h>
#include <types.h>

/* PATH_MAX is not defined in limits.h on some platforms */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct
{
  addr_t low;
  addr_t high;
  char name[PATH_MAX];
} MappedRegion;

//returns the number of mapped regions found
//these regions are placed in MappedRegion** regions
//this memory should be freed when it is no longer needed
//returns -1 if /proc/pid/maps could not be opened
int getMemoryMap(int pid,MappedRegion** regions);
