/*
  File: pmap.c
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
          Inspired by Albert Cahalan's pmap.c in procps
  Project:  katana
  Date: April 2010
  Description: Find out information about the target processes memory map.
               Requires a linux-like /proc filesystem. It should be useable
               on BSD and Solaris as well as Linux, but these have not been tested.
               FreeBSD seems to be getting rid of /proc

*/

#include "pmap.h"
#include <util/logging.h>



//returns the number of mapped regions found
//these regions are placed in MappedRegion** regions
//this memory should be freed when it is no longer needed
//returns -1 if /proc/pid/maps could not be opened
int getMemoryMap(int pid,MappedRegion** regions)
{
  char buf[64];
  snprintf(buf,64,"/proc/%i/maps",pid);
  FILE* f=fopen(buf,"r");
  if(!f)
  {
    logprintf(ELL_WARN,ELS_MISC,"Could not open %s\n",buf);
    return -1;
  }
  char linebuf[PATH_MAX+512];
  int numRegions=0;
  *regions=NULL;
  while(fgets(linebuf,PATH_MAX+512,f))
  {
    linebuf[strlen(linebuf)-1]='\0';//strip the trailing newline
    numRegions++;
    *regions=realloc(*regions,sizeof(MappedRegion)*numRegions);
    assert(*regions);
    memset(&(*regions)[numRegions-1],0,sizeof(MappedRegion));
    sscanf(linebuf,"%zx-%zx",&(*regions)[numRegions-1].low,&(*regions)[numRegions-1].high);
    char* path=strchr(linebuf,'/');
    if(path)
    {
      strncpy((*regions)[numRegions-1].name,path,PATH_MAX-1);
    }
    else
    {
      char* name="(no corresponding file)";
      strcpy((*regions)[numRegions-1].name,name);
    }
  }
  fclose(f);
  return numRegions;
}
