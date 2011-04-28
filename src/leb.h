/*
  File: leb.h
  Author: James Oakley
  Project:  katana
  Date: October 2010
  Description: utility functions for dealing with LEB-encoded numbers
*/

#ifndef leb_h
#define leb_h

#include "types.h"

//return value should be freed when you're finished with it
byte* encodeAsLEB128(byte* bytes,int numBytes,bool signed_,usint* numBytesOut);
//like encodeAsLEB128 except doesn't attempt to do any optimization
byte* encodeAsLEB128NoOptimization(byte* bytes,int numBytes,bool signed_,usint* numBytesOut);

//return value should be freed when you're finished with it
byte* decodeLEB128(byte* bytes,bool signed_,usint* numBytesOut,usint* numSeptetsRead);


uint leb128ToUInt(byte* bytes,usint* outLEBBytesRead);
int leb128ToInt(byte* bytes,usint* outLEBBytesRead);

//return value should be freed when caller is finished with it
byte* uintToLEB128(uint value,usint* numBytesOut);
byte* intToLEB128(int value,usint* numBytesOut);

word_t leb128ToUWord(byte* bytes,usint* outLEBBytesRead);
sword_t leb128ToSWord(byte* bytes,usint* outLEBBytesRead);

#endif
