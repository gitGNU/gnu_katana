/*
  File: rewrite.c
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
  Date: November 2010
  Description: Methods to rewrite a binary
*/

#ifndef rewrite_h
#define rewrite_h

//at the moment this function merely reads in an object and rewrites it out
void rewrite(ElfInfo* object,char* outfileName);

#endif
