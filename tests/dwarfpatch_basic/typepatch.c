/*
  File: typepatch.c
  Author: James Oakley
  Project: Katana (preliminary work)
  Date: January 10
  Description: Preliminary patching program just for patching main_v0.c's structure Foo to have an extra field
               The first version of this program is going to be very simple. It will perform the following steps
               1. Load main_v0 with dwarfdump and determine where the variable bar is stored
               2. Find all references to the variable bar
               3. Attach to the running process with ptrace
               4. Make the process execute mmap to allocate space for a new data segment
               5. Copy over field1 and field2 from Foo bar into the new memory area and zero the added field
               6. Fixup all locations referring to the old address of bar with the new address and set the offset accordingly (should be able to get information for fixups from the rel.text section)
  Usage: typepatch PID
         PID is expected to be a running process build from main_v0.c
*/

#include <libelf.h>
#include <gelf.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc,char** argv)
{
  Elf* e;
  if(elf_version(EV_CURRENT)==EV_NONE)
  {
    fprintf(stderr,"Failed to init ELF library\n");
    exit(1);
  }
  int fd=open("patchee",O_RDWR);
  if(fd < 0)
  {
    fprintf(stderr,"Failed to open binary to be patched\n");
    exit(1);
  }
  e=elf_begin(fd,ELF_C_RDWR,NULL);
  if(!e)
  {
    fprintf(stderr,"Failed to open as an ELF file %s\n",elf_errmsg(-1));
    exit(1);
  }

  //all done
  elf_end(e);
  close(fd);
  return 0;
}
