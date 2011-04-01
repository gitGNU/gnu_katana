/*
  File: logging.c
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

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html

  Project: Katana
  Date: February 2010
  Description: Interface for logging information, errors, etc
*/
#include "logging.h"
#include <stdarg.h>
#include "util.h"
#include <stdbool.h>
#include <assert.h>

E_LOG_LEVEL lvlEnabled=ELL_INFO_V4;
E_LOG_LEVEL sourceEnables[ELS_CNT];

int logprintf(E_LOG_LEVEL lvl,E_LOG_SOURCE src,const char* fmt,...)
{

  if(lvl > sourceEnables[src] || lvl>lvlEnabled)
  {
    return 0;
  }
  int retval=0;
  va_list ap;
  va_start(ap,fmt);
  if(ELL_ERR==lvl)
  {
    fprintf(stderr,"ERROR: ");
    if(src)
    {
      vfprintf(stderr,fmt,ap);
    }
    death(NULL);
  }
  else if(ELL_WARN==lvl)
  {
    fprintf(stderr,"WARNING: ");
    if(fmt)
    {
      retval=vfprintf(stderr,fmt,ap);
    }
  }
  else
  {
    retval=vprintf(fmt,ap);
  }
  va_end(ap);
  return retval;
}

//all messages at this level or more important than this level (see the ordering in
//E_LOG_LEVEL definition) will be logged. Others will be dropped
void setMasterLogLevel(E_LOG_LEVEL lvl)
{
  lvlEnabled=lvl;
}

void enableLogSource(E_LOG_SOURCE src,E_LOG_LEVEL lvl)
{
  assert(0<=src && src<ELS_CNT);
  sourceEnables[src]=lvl;
}

void disableLogSource(E_LOG_SOURCE src)
{
  enableLogSource(src,ELL_DISABLE);
}

void loggingDefaults()
{
  lvlEnabled=ELL_INFO_V4;
  sourceEnables[ELS_MISC]=ELL_WARN;
  sourceEnables[ELS_CODEDIFF]=ELL_WARN;
  sourceEnables[ELS_TYPEDIFF]=ELL_WARN;
  sourceEnables[ELS_DWARF_FRAME]=ELL_INFO_V1;
  sourceEnables[ELS_HOTPATCH]=ELL_INFO_V1;
  sourceEnables[ELS_SOURCETREE]=ELL_WARN;
  sourceEnables[ELS_SYMBOL]=ELL_WARN;
  sourceEnables[ELS_RELOCATION]=ELL_WARN;
  sourceEnables[ELS_PATCHAPPLY]=ELL_CNT;
  sourceEnables[ELS_LINKMAP]=ELL_INFO_V4;
  sourceEnables[ELS_DWARFTYPES]=ELL_WARN;
  sourceEnables[ELS_SAFETY]=ELL_INFO_V1;
  sourceEnables[ELS_PATH]=ELL_CNT;
  sourceEnables[ELS_ELFWRITE]=ELL_INFO_V2;
  sourceEnables[ELS_PATCHWRITE]=ELL_WARN;
  sourceEnables[ELS_VERSION]=ELL_WARN;
  sourceEnables[ELS_DWARFWRITE]=ELL_INFO_V2;
  sourceEnables[ELS_LEB]=ELL_INFO_V3;
  sourceEnables[ELS_CONFIG]=ELL_INFO_V4;
  sourceEnables[ELS_SHELL]=ELL_INFO_V4;
  sourceEnables[ELS_DWARFSCRIPT]=ELL_INFO_V4;
  sourceEnables[ELS_DWARF_BUILD]=ELL_INFO_V2;
  sourceEnables[ELS_VM]=ELL_INFO_V4;
  sourceEnables[ELS_CLEANUP]=ELL_INFO_V1;
}
