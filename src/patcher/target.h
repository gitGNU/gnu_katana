/*
  File: target.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
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
  Date: January 10
  Description: prototypes for functions to operate on an in-memory target
*/

#ifndef target_h
#define target_h
#include <sys/types.h>
#include <sys/user.h>
#ifdef __USE_MISC
#include <sys/mman.h>
#else
#define __USE_MISC
#include <sys/mman.h>
#undef __USE_MISC
#endif
#include <sys/syscall.h>
#include "types.h"
#include "arch.h"
//#include <sys/user.h>

//this must be called before any other functions in this file
void startPtrace(int pid);

void continuePtrace();
void endPtrace(bool stopProcess);
void modifyTarget(addr_t addr,word_t value);
//copies numBytes from data to addr in target
//todo: does addr have to be aligned
void memcpyToTarget(addr_t addr,byte* data,int numBytes);
//copies numBytes to data from addr in target
//todo: does addr have to be aligned
void memcpyFromTarget(byte* data,long addr,int numBytes);

//like memcpyFromTarget except doesn't kill katana
//if ptrace fails
//returns true if it succeseds
bool memcpyFromTargetNoDeath(byte* data,long addr,int numBytes);

void getTargetRegs(struct user_regs_struct* regs);
void setTargetRegs(struct user_regs_struct* regs);
//allocate a region of memory in the target
//return the address (in the target) of the region
//or NULL if the operation failed
addr_t mmapTarget(word_t size,int prot,addr_t desiredAddress);

//must be called before any calls to mallocTarget
void setMallocAddress(addr_t addr);
//must be called before any calls to mallocTarget
void setTargetTextStart(addr_t addr);
addr_t mallocTarget(word_t len);

//compare a string to a string located
//at a certain address in the target
//return true if the strings match
//up to strlen(str) characters
bool strnmatchTarget(char* str,addr_t strInTarget);

void setBreakpoint(addr_t loc);
void removeBreakpoint(addr_t loc);
#endif
