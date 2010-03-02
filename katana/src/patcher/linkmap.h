/*
  File: linkmap.c
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: This file contains methods to facilitate finding functions that have been
               dynamically linked into the running target. It does this by examining the
               linkmaps of the target directly
  Attribution: this code is inspired by a linkmap method
               posted by the grugq to bugtraq in the post "More ELF buggery"
               http://seclists.org/bugtraq/2002/May/295
               The same technique was also known to Julien Vanegue,
               Author of Eresi. It is likely that this method may be part
               of Eresi's linkmap code although I have not dug into Eresi's
               linkmap code to find out the precise method used there.
*/


#ifndef linkmap_h
#define linkmap_h
//the passed ElfInfo object must correspond to the
//currently running target known to the methods
//in target.c
addr_t locateRuntimeSymbolInTarget(ElfInfo* e,char* name);
#endif
