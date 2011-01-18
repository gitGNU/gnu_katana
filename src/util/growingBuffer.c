/*
  File: growingBuffer.c
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

#include "growingBuffer.h"
#include "leb.h"

void addToGrowingBuffer(GrowingBuffer* buf,void* data,int dataLen)
{
  if(!dataLen)
  {
    return;
  }
  if(buf->len+dataLen > buf->allocated)
  {
    buf->allocated=(buf->len+dataLen)*2;
    buf->data=realloc(buf->data,buf->allocated);
  }
  memcpy(buf->data+buf->len,data,dataLen);
  buf->len+=dataLen;
}

void addUlebToGrowingBuffer(GrowingBuffer* buf,word_t data)
{
  usint numBytesOut;
  byte* leb=encodeAsLEB128((byte*)&data,sizeof(data),false,&numBytesOut);
  addToGrowingBuffer(buf,leb,numBytesOut);
  free(leb);
}

void addSlebToGrowingBuffer(GrowingBuffer* buf,sword_t data)
{
  usint numBytesOut;
  byte* leb=encodeAsLEB128((byte*)&data,sizeof(data),true,&numBytesOut);
  addToGrowingBuffer(buf,leb,numBytesOut);
  free(leb);
}

