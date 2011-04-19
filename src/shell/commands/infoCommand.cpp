/*
  File: shell/commands/infoCommand.cpp
  Author: James Oakley
  Copyright (C): 2010 James Oakley
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version.

  This file was not written while under employment by Dartmouth
  College and the attribution requirements on the rest of Katana do
  not apply to code taken from this file.
  Project:  katana
  Date: December 2010
  Description: Katana shell command to print out info about an ELF object
*/

#include "infoCommand.h"
extern "C"
{
#include "info/fdedump.h"
#include "info/dwinfo_dump.h"
#include "info/unsafe_funcs_dump.h"
}

InfoCommand::InfoCommand(InfoOperation op,ShellParam* elfObject,ShellParam* outfile)
  :op(op),elfObjectP(elfObject),outfileP(outfile)
{
  this->elfObjectP->grab();
  if(this->outfileP)
  {
    this->outfileP->grab();
  }
}  
InfoCommand::~InfoCommand()
{
  elfObjectP->drop();
  if(outfileP)
  {
    outfileP->drop();
  }
}
void InfoCommand::execute()
{
  switch(op)
  {
  case IOP_EXCEPTION:
    {
      ElfInfo* elf=elfObjectP->getElfObject();
      if(!elf)
      {
        logprintf(ELL_WARN,ELS_SHELL,"Cannot write info out because first parameter was not an ELF object\n");
      }
      FILE* outfile=stdout;
      if(outfileP)
      {
        char* outfileName=outfileP->getString();
        if(!outfileName)
        {
          logprintf(ELL_WARN,ELS_SHELL,"Cannot write info out because outfile name parameter was not a string\n");
          return;
        }
        outfile=fopen(outfileName,"w");
        if(!outfile)
        {
          logprintf(ELL_WARN,ELS_SHELL,"Cannot open file %s for writing info output to to\n",outfileName);
          return;
        }
      }
      readDebugFrame(elf,true);
      printCallFrameInfo(outfile,&elf->callFrameInfo,elf);
      //todo: print gcc except table
    }
    break;
  case IOP_PATCH:
    {
      ElfInfo* elf=elfObjectP->getElfObject();
      if(!elf)
      {
        logprintf(ELL_WARN,ELS_SHELL,"Cannot write info out because first parameter was not an ELF object\n");
      }
      if(!elf->isPO)
      {
        throw "ELF object is not a patch";
      }
      Map* fdeMap=readDebugFrame(elf,false);
      printf("*********Type and Function Info****************\n");
      printPatchDwarfInfo(elf,fdeMap);
      printf("\n*********Safety Info****************\n");
      printPatchUnsafeFuncsInfo(elf);
      printf("\n*********Type Transformation Rules****************\n");
      printCallFrameInfo(stdout,&elf->callFrameInfo,elf);
      break;
    }
  default:
    death("Unknown info sub-command\n");
  }
}


