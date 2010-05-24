/*
  File: config.h
  Author: James Oakley
  Written For: Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: February 2010
  Description: Program environment, options that can be configured
*/

#ifndef config_h
#define config_h
#include <stdbool.h>

typedef enum
{
  EKCF_CHECK_PTRACE_WRITES,//check writes done into target memory
  EKCF_COUNT
} E_KATANA_CONFIG_FLAGS;
void setDefaultConfig();//called on startup
bool isFlag(E_KATANA_CONFIG_FLAGS flag);
void setFlag(E_KATANA_CONFIG_FLAGS flag,bool state);

#endif
