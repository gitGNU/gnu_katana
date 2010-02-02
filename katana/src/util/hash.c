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


//! this one is called djb2
unsigned long hash1(char* str) {
  unsigned long hash = 5381;
  int c;
  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

//hash function taken from http://www.concentric.net/~Ttwang/tech/inthash.htm
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

