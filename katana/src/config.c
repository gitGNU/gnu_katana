/*
  File: config.h
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: Program environment, options that can be configured
*/
#include "config.h"
#include <assert.h>

bool flags[EKCF_COUNT];

void setDefaultConfig()
{
  setFlag(EKCF_CHECK_PTRACE_WRITES,true);
}

bool isFlag(E_KATANA_CONFIG_FLAGS flag)
{
  assert(flag>=0 && flag<EKCF_COUNT);
  return flags[flag];
}

void setFlag(E_KATANA_CONFIG_FLAGS flag,bool state)
{
  assert(flag>=0 && flag<EKCF_COUNT);
  flags[flag]=state;
}
