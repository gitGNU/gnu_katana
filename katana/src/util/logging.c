/*
  File: logging.h
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: Interface for logging information, errors, etc
*/
#include "logging.h"
#include <stdarg.h>
#include "util.h"
#include <stdbool.h>
#include <assert.h>

E_LOG_LEVEL lvlEnabled;
bool sourceEnables[ELS_CNT];

int logprintf(E_LOG_LEVEL lvl,E_LOG_SOURCE src,char* fmt,...)
{

  if(!sourceEnables[src] || lvl>lvlEnabled)
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
    if(src)
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
void setLogLevel(E_LOG_LEVEL lvl);

void enableLogSource(E_LOG_SOURCE src)
{
  assert(0<=src && src<ELS_CNT);
  sourceEnables[src]=true;
}

void disableLogSource(E_LOG_SOURCE src)
{
  assert(0<=src && src<ELS_CNT);
  sourceEnables[src]=true;
}

void loggingDefaults()
{
  lvlEnabled=ELL_INFO_V3;
  sourceEnables[ELS_MISC]=true;
  sourceEnables[ELS_CODEDIFF]=true;
}
