// File: hash.h
//
// This is some utilites for CS23 search engine project ( or general case)
//
// Three hash funtions interface

#ifndef _HASH_H__
#define _HASH_H__

#include <stdint.h> //for uint32_t and uint64_t


#if __WORDSIZE==64
//#define hashSizeT(a) hash64Bit(a)
#define hashSizeT    hash64Bit
#else
//#define hashSizeT(a) hash32Bit(a)
#define hashSizeT    hash32Bit
#endif

unsigned long hash1(char*);

unsigned long hashInt(int);
uint32_t hash32Bit(uint32_t key);
uint64_t hash64Bit(uint64_t key);
#endif
