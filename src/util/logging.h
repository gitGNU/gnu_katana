/*
  File: logging.h
  Author: James Oakley
  Project:  katana
  Date: February 2010
  Description: Interface for logging information, errors, etc
*/

#ifndef logging_h
#define logging_h

typedef enum
{
  ELL_DISABLE=0,//exists for the purpose of something lower than all log levels, should never be used in a call to logprintf
  ELL_ERR,
  ELL_WARN,
  ELL_INFO_V1,
  ELL_INFO_V2,
  ELL_INFO_V3, //higher numbers are less important
  ELL_INFO_V4,
  ELL_CNT,
} E_LOG_LEVEL;

typedef enum
{
  ELS_MISC=0,
  ELS_DWARFTYPES,
  ELS_CODEDIFF,
  ELS_TYPEDIFF,
  ELS_DWARF_FRAME,
  ELS_HOTPATCH,
  ELS_SOURCETREE,
  ELS_SYMBOL,
  ELS_RELOCATION,
  ELS_PATCHAPPLY,
  ELS_LINKMAP,
  ELS_SAFETY,
  ELS_PATCHWRITE,
  ELS_PATH,
  ELS_ELFWRITE,
  ELS_CNT
} E_LOG_SOURCE;//describes what subsystem the message came from

int logprintf(E_LOG_LEVEL lvl,E_LOG_SOURCE src,char* fmt,...);

//all messages at this level or more important than this level (see the ordering in
//E_LOG_LEVEL definition) will be logged. Others will be dropped
void setMasterLogLevel(E_LOG_LEVEL lvl);

void enableLogSource(E_LOG_SOURCE src,E_LOG_LEVEL lvl);
void disableLogSource(E_LOG_SOURCE src);

void loggingDefaults();
#endif
