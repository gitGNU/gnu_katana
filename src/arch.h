/*
  File: arch.h
  Author: James Oakley
  Project:  katana
  Date: March 2010
  Description: Macros, etc, for handling architecture-specific stuff like 32-bit vs 64-bit
               Do note that i386 and x86_64 are the only support architectures
*/

#include <sys/user.h>

#if __WORDSIZE == 64
#define REG_IP(regs_struct) (regs_struct).rip
#define REG_SP(regs_struct) (regs_struct).rsp
#define REG_AX(regs_struct) (regs_struct).rax
#define REG_BX(regs_struct) (regs_struct).rbx
#define REG_CX(regs_struct) (regs_struct).rcx
#define REG_DX(regs_struct) (regs_struct).rdx
#define REG_BP(regs_struct) (regs_struct).rbp
#define REG_SI(regs_struct) (regs_struct).rsi
#define REG_DI(regs_struct) (regs_struct).rdi
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
#define REG_IP(regs_struct) (regs_struct).eip
#define REG_SP(regs_struct) (regs_struct).esp
#define REG_AX(regs_struct) (regs_struct).eax
#define REG_BX(regs_struct) (regs_struct).ebx
#define REG_CX(regs_struct) (regs_struct).ecx
#define REG_DX(regs_struct) (regs_struct).edx
#define REG_BP(regs_struct) (regs_struct).ebp
#define REG_SI(regs_struct) (regs_struct).esi
#define REG_DI(regs_struct) (regs_struct).edi
#define ElfXX_Sym Elf32_Sym
#define ElfXX_Rel Elf32_Rel
#define ElfXX_Rela Elf32_Rela
#define ElfXX_Shdr Elf32_Shdr
#define ElfXX_Ehdr Elf32_Ehdr
#define ElfXX_Dyn Elf64_Dyn
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
