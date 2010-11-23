/*
  File: arch.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: Katana is free software: you may redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 2 of the
    License, or (at your option) any later version. Regardless of
    which version is chose, the following stipulation also applies:
    
    Any redistribution must include copyright notice attribution to
    Dartmouth College as well as the Warranty Disclaimer below, as well as
    this list of conditions in any related documentation and, if feasible,
    on the redistributed software; Any redistribution must include the
    acknowledgment, “This product includes software developed by Dartmouth
    College,” in any related documentation and, if feasible, in the
    redistributed software; and The names “Dartmouth” and “Dartmouth
    College” may not be used to endorse or promote products derived from
    this software.  

                             WARRANTY DISCLAIMER

    PLEASE BE ADVISED THAT THERE IS NO WARRANTY PROVIDED WITH THIS
    SOFTWARE, TO THE EXTENT PERMITTED BY APPLICABLE LAW. EXCEPT WHEN
    OTHERWISE STATED IN WRITING, DARTMOUTH COLLEGE, ANY OTHER COPYRIGHT
    HOLDERS, AND/OR OTHER PARTIES PROVIDING OR DISTRIBUTING THE SOFTWARE,
    DO SO ON AN "AS IS" BASIS, WITHOUT WARRANTY OF ANY KIND, EITHER
    EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE
    SOFTWARE FALLS UPON THE USER OF THE SOFTWARE. SHOULD THE SOFTWARE
    PROVE DEFECTIVE, YOU (AS THE USER OR REDISTRIBUTOR) ASSUME ALL COSTS
    OF ALL NECESSARY SERVICING, REPAIR OR CORRECTIONS.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL DARTMOUTH COLLEGE OR ANY OTHER COPYRIGHT HOLDER, OR ANY OTHER
    PARTY WHO MAY MODIFY AND/OR REDISTRIBUTE THE SOFTWARE AS PERMITTED
    ABOVE, BE LIABLE TO YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OR
    INABILITY TO USE THE SOFTWARE (INCLUDING BUT NOT LIMITED TO LOSS OF
    DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR
    THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

    The complete text of the license may be found in the file COPYING
    which should have been distributed with this software. The GNU
    General Public License may be obtained at
    http://www.gnu.org/licenses/gpl.html
    
  Project:  Katana
  Date: March 2010
  Description: Macros, etc, for handling architecture-specific stuff like 32-bit vs 64-bit
               Do note that i386 and x86_64 are the only support architectures
*/

#include <sys/user.h>

#if __WORDSIZE == 64
#define KATANA_X86_64_ARCH
#define REG_IP(regs_struct) (regs_struct).rip
#define REG_SP(regs_struct) (regs_struct).rsp
#define REG_AX(regs_struct) (regs_struct).rax
#define REG_BX(regs_struct) (regs_struct).rbx
#define REG_CX(regs_struct) (regs_struct).rcx
#define REG_DX(regs_struct) (regs_struct).rdx
#define REG_BP(regs_struct) (regs_struct).rbp
#define REG_SI(regs_struct) (regs_struct).rsi
#define REG_DI(regs_struct) (regs_struct).rdi
#define REG_8(regs_struct) (regs_struct).r8
#define REG_9(regs_struct) (regs_struct).r9
#define REG_10(regs_struct) (regs_struct).r10
#define REG_11(regs_struct) (regs_struct).r10
#define REG_12(regs_struct) (regs_struct).r10
#define REG_13(regs_struct) (regs_struct).r10
#define REG_14(regs_struct) (regs_struct).r10
#define REG_15(regs_struct) (regs_struct).r10
#define NUM_REGS 15
#define ElfXX_Sym Elf64_Sym
#define ElfXX_Rel Elf64_Rel
#define ElfXX_Rela Elf64_Rela
#define ElfXX_Shdr Elf64_Shdr
#define ElfXX_Ehdr Elf64_Ehdr
#define ElfXX_Dyn Elf64_Dyn
#define elfxx_getshdr elf64_getshdr
#define elfxx_newehdr elf64_newehdr
#define ELFXX_R_TYPE ELF64_R_TYPE
#define ELFXX_R_SYM ELF64_R_SYM
#define ELFXX_R_INFO ELF64_R_INFO
#define ELFXX_ST_BIND ELF64_ST_BIND
#define ELFXX_ST_TYPE ELF64_ST_TYPE
#define ELFXX_ST_INFO ELF64_ST_INFO
#define ELFCLASSXX ELFCLASS64
#define ElfXX_Word Elf64_Word
#define PTRACE_WORD_SIZE 8
#elif __WORDSIZE == 32
#define KATANA_X86_ARCH
#define REG_IP(regs_struct) (regs_struct).eip
#define REG_SP(regs_struct) (regs_struct).esp
#define REG_AX(regs_struct) (regs_struct).eax
#define REG_BX(regs_struct) (regs_struct).ebx
#define REG_CX(regs_struct) (regs_struct).ecx
#define REG_DX(regs_struct) (regs_struct).edx
#define REG_BP(regs_struct) (regs_struct).ebp
#define REG_SI(regs_struct) (regs_struct).esi
#define REG_DI(regs_struct) (regs_struct).edi
#define NUM_REGS 8
#define ElfXX_Sym Elf32_Sym
#define ElfXX_Rel Elf32_Rel
#define ElfXX_Rela Elf32_Rela
#define ElfXX_Shdr Elf32_Shdr
#define ElfXX_Ehdr Elf32_Ehdr
#define ElfXX_Dyn Elf32_Dyn
#define elfxx_getshdr elf32_getshdr
#define elfxx_newehdr elf32_newehdr
#define ELFXX_R_TYPE ELF32_R_TYPE
#define ELFXX_R_SYM ELF32_R_SYM
#define ELFXX_R_INFO ELF32_R_INFO
#define ELFXX_ST_BIND ELF32_ST_BIND
#define ELFXX_ST_TYPE ELF32_ST_TYPE
#define ELFXX_ST_INFO ELF32_ST_INFO
#define ELFCLASSXX ELFCLASS32
#define ElfXX_Word Elf32_Word
#define PTRACE_WORD_SIZE 4
#else
#error Unknown word size (not 32-bit or 64-bit)
#endif
