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
  ELL_ERR=0,
  ELL_WARN,
  ELL_INFO_V1,
  ELL_INFO_V3, //higher numbers are less important
  ELL_INFO_V4,
  ELL_CNT
} E_LOG_LEVEL;

typedef enum
{
  ELS_MISC,
  ELS_CODEDIFF,
  ELS_CNT
} E_LOG_SOURCE;//describes what subsystem the message came from

int logprintf(E_LOG_LEVEL lvl,E_LOG_SOURCE src,char* fmt,...);

//all messages at this level or more important than this level (see the ordering in
//E_LOG_LEVEL definition) will be logged. Others will be dropped
void setLogLevel(E_LOG_LEVEL lvl);

void enableLogSource(E_LOG_SOURCE src);
void disableLogSource(E_LOG_SOURCE src);

void loggingDefaults();
#endif
