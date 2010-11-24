/*
  File: config.h
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
    
  Project:  Katana
  Date: February 2010
  Description: Program environment, options that can be configured
*/

#ifndef config_h
#define config_h
#include <stdbool.h>

//convention: EKCF_P refers to a flag that only affects patch application mode
//            EKCF_G refers to a flag that only affects patch generation mode
//            EKCF_I refers to a flag that only affects patch info listing mode
typedef enum
{
  EKCF_CHECK_PTRACE_WRITES=0,//check writes done into target memory
  EKCF_P_STOP_TARGET, //stop the target process when finished applying
                      //the patch. This is only used for debugging
  EKCF_EH_FRAME,   //instead of listing call frame info from
                   //.debug_frame, list it from .eh_frame
  EKCF_COUNT
} E_KATANA_CONFIG_FLAGS;
extern const char* flagNames[];


//katana modes of operation
typedef enum
{
  EKM_NONE,
  EKM_GEN_PATCH,
  EKM_APPLY_PATCH,
  EKM_INFO,   //prints out info about a file
  EKM_TEST_PASSTHROUGH, //reads executable w/ DWARF and writes it back out again
} E_KATANA_MODE;


struct Config
{
  // The maximum number of seconds to wait for the target to enter a safe state.
  int maxWaitForPatching;
  E_KATANA_MODE mode;//the mode katana is operating in right now
  char* outfileName;//the name of the file to write out to. Mostly
                    //used in PATCHGEN mode
  char* oldSourceTree;//for patch generation, where the old object files are
  char* newSourceTree;//for patch generation, where the new object files are
  char* objectName;//for patch generation, where the executable file
                   //is for each version relative to the source
                   //tree. for patch application, the patch file to load
  int pid;         //for patch application, the process to attach to
  
};

//for global access to the configuration
extern struct Config config;

void setDefaultConfig();//called on startup
bool isFlag(E_KATANA_CONFIG_FLAGS flag);
void setFlag(E_KATANA_CONFIG_FLAGS flag,bool state);
void loadConfigurationFile(char* fname);

#endif
