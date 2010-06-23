/*
  File: pmap.c
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

          Inspired by Albert Cahalan's pmap.c in procps
    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: April 2010
  Description: Find out information about the target processes memory map.
               Requires a linux-like /proc filesystem. It should be useable
               on BSD and Solaris as well as GNU/Linux, but these have not been tested.
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
