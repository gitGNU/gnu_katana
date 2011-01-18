/*
  File: growingBuffer.h
  Author: James Oakley
  Copyright (C): 2011 James Oakley
  License: Katana is free software: you may redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation, either version 2 of the
  License, or (at your option) any later version.

  This file was not written while under employment by Dartmouth
  College and the attribution requirements on the rest of Katana do
  not apply to code taken from this file.
  Project:  katana
  Date: January 2011
  Description: Utility methods that need to be implemented in C++
*/

#ifndef growing_buffer_h
#define growing_buffer_h

#include "types.h"

typedef struct
{
  byte* data;
  int len;
  int allocated;
} GrowingBuffer;

void addToGrowingBuffer(GrowingBuffer* buf,void* data,int dataLen);
void addUlebToGrowingBuffer(GrowingBuffer* buf,word_t data);
void addSlebToGrowingBuffer(GrowingBuffer* buf,sword_t data);

#endif
