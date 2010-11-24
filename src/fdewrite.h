/*
  File: fdewrite.h
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
  Description: Methods to transform internal representation of CIE/FDE
  data back to its disk representation
*/


#ifndef fdewrite_h
#define fdewrite_h

//transform a CIE into its binary form for writing to disk
void writeCIEToDwarf(Dwarf_P_Debug dbg,CIE* cie);

//transform an FDE into its binary form for writing to disk
void writeFDEToDwarf(Dwarf_P_Debug dbg,FDE* fde);

#endif
