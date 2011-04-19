/*
  File: shell/commands/infoCommand.h
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

#ifndef info_command_h
#define info_command_h

#include "command.h"

typedef enum
{
  IOP_EXCEPTION=1,
  IOP_PATCH,
} InfoOperation;

class InfoCommand : public ShellCommand
{
public:
  InfoCommand(InfoOperation op,ShellParam* elfObject,ShellParam* outfile=NULL);
  ~InfoCommand();
  virtual void execute();
protected:
  InfoOperation op;
  ShellParam* elfObjectP;
  ShellParam* outfileP;
};

#endif
// Local Variables:
// mode: c++
// End:
