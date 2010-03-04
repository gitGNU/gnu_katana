// File: hash.h
// Description: Includes a number of sample hash functions.
// 
//! This is some utilites for CS23 search engine project ( or general case)
//! \brief implementation of the hash
//!        originally from http://www.cse.yorku.ca/~oz/hash.html
//! Hash functions are not randomly designed, there are theories behind.
//! read the above web page for details.

#include <string.h>
#include <stdlib.h>
#include "hash.h"
#include <assert.h>

//! this one is called djb2
unsigned long hash1(char* str) {
  assert(str);
  unsigned long hash = 5381;
  int c;
  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

unsigned long hashInt(int key)
{
  unsigned int res = ~key + (key << 15);
  res = res ^ (res >> 12);
  res = res + (res << 2);
  res = res ^ (res >> 4);
  res = res * 2057;
  res = res ^ (res >> 16);
  return res;
}

//hash function taken from http://www.concentric.net/~Ttwang/tech/inthash.htm
uint32_t hash32Bit(uint32_t key)
{
  uint32_t res = ~key + (key << 15);
  res = res ^ (res >> 12);
  res = res + (res << 2);
  res = res ^ (res >> 4);
  res = res * 2057;
  res = res ^ (res >> 16);
  return res;
}

//hash function taken from http://www.concentric.net/~Ttwang/tech/inthash.htm
uint64_t hash64Bit(uint64_t key)
{
  key = (~key) + (key << 21); // key = (key << 21) - key - 1;
  key = key ^ (key >> 24);
  key = (key + (key << 3)) + (key << 8); // key * 265
  key = key ^ (key >> 14);
  key = (key + (key << 2)) + (key << 4); // key * 21
  key = key ^ (key >> 28);
  key = key + (key << 31);
  return key;
}
