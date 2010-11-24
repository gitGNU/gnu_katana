/*
  File: fdewrite.c
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



//transform a CIE into its binary form for writing to disk
void writeCIEToDwarf(Dwarf_P_Debug dbg,CIE* cie)
{
  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(cie->initialInstructions,
                                  cie->numInitialInstructions);
  Dwarf_Unsigned cieID;
  cieID=dwarf_add_frame_cie(dbg,cie->augmentation,cie->codeAlign,cie->dataAlign,cie->returnAddrRuleNum,rawInstructions->instrs,rawInstructions->numBytes,&err);
  if(DW_DLV_NOCOUNT==cieID)
  {
    dwarfErrorHandler(err,"creating frame cie failed");
  }
  cie->idx=cieID;
  destroyRawInstructions(instrs);
}

//transform an FDE into its binary form for writing to disk
void writeFDEToDwarf(Dwarf_P_Debug dbg,FDE* fde)
{
  DwarfInstructions rawInstructions=
    serializeDwarfRegInstructions(fde->instructions,
                                  fde->numInstructions);
  Dwarf_P_Fde dwFde=dwarf_new_fde(dbg,&err);
  int fdeIdx=dwarf_add_frame_fde(dbg,dwFde,NULL,fde->cie->idx,fde->lowpc,fde->highpc-fde->lowpc,0,&err);
  dwarf_insert_fde_inst_bytes(dbg,fdeIdx,instrs.numBytes,instrs.instrs,&err);
  //todo: is this ok? Might we be overwriting a value already there
  //from the file we loaded from that we care about
  fde->idx=fdeIdx;

}




