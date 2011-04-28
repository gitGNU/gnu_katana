/*
  File: callFrameInfo.c
  Author: James Oakley
  Copyright (C): 2010-2011 Dartmouth College
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

  Project: Katana
  Date: April, 2011
  Description: Data structures and methods dealing with information from .gcc_except_table
*/

#include "callFrameInfo.h"
#include "constants.h"
#include "leb.h"
#include "util/logging.h"
#include "elfutil.h"
#include "util/growingBuffer.h"
#include "eh_pe.h"

//the lsdaPointers parameter is used to store the transformation from an LSDA
//index to the actual address that the LSDA lives at. This will be necessary when writing the FDEs
void buildExceptTableRawData(CallFrameInfo* cfi,GrowingBuffer* buf,
                             addr_t** lsdaPointers)
{
  ExceptTable* table=cfi->exceptTable;
  assert(table);
  *lsdaPointers=zmalloc(table->numLSDAs*sizeof(addr_t));
  for(int i=0;i<table->numLSDAs;i++)
  {
    //update the map
    (*lsdaPointers)[i]=cfi->exceptTableAddress+buf->len;
    
    //now get on with actually building the binary data
    LSDA* lsda=table->lsdas+i;
    addr_t lsdaStartOffset=buf->len;
    //gcc hasn't been writing lpstart (other than DW_PE_omit for the
    //encoding) so I'm guessing at proper behaviour here.
    if(lsda->lpStart!=0)
    {
      byte enc=DW_EH_PE_uleb128;
      addToGrowingBuffer(buf,&enc,1);
      addUlebToGrowingBuffer(buf,lsda->lpStart);
    }
    else
    {
      byte enc=DW_EH_PE_omit;
      addToGrowingBuffer(buf,&enc,1);
    }
    //encoding used by entries in the type table
    addToGrowingBuffer(buf,&lsda->ttEncoding,1);


    //unfortunately we now run into a problem. We need to write out
    //the TType base offset as a uleb128. LEB values vary in length,
    //but we don't yet know what the offset is so we can't tell how
    //long it will be. We solve this with the inefficient solution of
    //creating a second buffer which we will copy into the first
    //later. We also don't know how long the call site table will be

    //in order to properly create parts of the call-site table,
    //however, we need the action table. 
    GrowingBuffer actionBuf;
    ZERO(actionBuf);
    addr_t* actionIdxToOffset=zmalloc(sizeof(addr_t)*lsda->numActionEntries);
    for(int j=0;j<lsda->numActionEntries;j++)
    {
      if(actionIdxToOffset[j])
      {
        //we already wrote this action. Skip it now.
        continue;
      }
      int idx=j;
      while(true)
      {
        actionIdxToOffset[idx]=actionBuf.len;
        ActionRecord* action=lsda->actionTable+idx;
        //note we make the index 1-based when writing it out
        addUlebToGrowingBuffer(&actionBuf,action->typeFilterIndex+1);
        if(action->hasNextAction)
        {
          if(actionIdxToOffset[action->nextAction] || action->nextAction==0)
          {
            //the next action is an action we've already written out. Construct a self-relative offset.
            sword_t offset=(sword_t)actionIdxToOffset[action->nextAction] - (sword_t)actionBuf.len;
            addSlebToGrowingBuffer(&actionBuf,offset);
            //don't need to follow the chain any further, it's already been written out.
            break;
          }
          else
          {
            //we'll just choose to write the next action next. In that case the offset is 1 byte.
            byte offset=1;
            addToGrowingBuffer(&actionBuf,&offset,1);
            //the next iteration of this while loop will continue writing this chain
            idx=action->nextAction;
          }
        }
        else
        {
          //just write a zero to show there is no next action
          byte nextAction=0;
          addToGrowingBuffer(&actionBuf,&nextAction,1);
          //and don't need to write out any actions in this chain.
          break;
        }
      }
    }

    /////////////////////////////////
    //write out the call site table
    GrowingBuffer callSiteBuf;
    ZERO(callSiteBuf);
    for(int j=0;j<lsda->numCallSites;j++)
    {
      CallSiteRecord* callSite=&lsda->callSiteTable[j];
      addUlebToGrowingBuffer(&callSiteBuf,callSite->position);
      addUlebToGrowingBuffer(&callSiteBuf,callSite->length);
      addUlebToGrowingBuffer(&callSiteBuf,callSite->landingPadPosition);
      if(callSite->hasAction)
      {
        if(callSite->firstAction >= lsda->numActionEntries)
        {
          death("Invalid firstAction for call site, index is too high\n");
        }
        //note that we make the offset one more than it really is
        //because of gcc convention to tell whether or not there is an
        //action at all
        addUlebToGrowingBuffer(&callSiteBuf,1+actionIdxToOffset[callSite->firstAction]);
      }
      else
      {
        byte noAction=0;
        addToGrowingBuffer(&callSiteBuf,&noAction,1);
      }
    }


    //defer writing out the type table for now so we can write out
    //pc-relative type table stuff more easily
    int ttEntrySize=getPointerSizeFromEHPointerEncoding(lsda->ttEncoding);
    int ttSize=ttEntrySize*lsda->numTypeEntries;

    /////////////////////////////////////
    //ok, now we have the messy problem of calculating the offset of
    //the end of the type table from the location at which we write
    //the offset of the type table. This is icky because it depends on
    //the alignment of the type table, which must be 4-byte aligned
    //and that alignment depends on how many bytes are taken up by
    //writing the offset. We kludgily solve this by always making sure
    //we have extra space to move things around by a few bytes.
    //todo: can avoid this calculation if we're not actually going to
    //write the type table


    usint callSiteLengthLEBNumBytes;
    byte* callSiteLengthLEB=encodeAsLEB128((byte*)&callSiteBuf.len,sizeof(callSiteBuf.len),false,&callSiteLengthLEBNumBytes);

    addr_t ttBaseOffsetGuess=1+callSiteLengthLEBNumBytes+callSiteBuf.len+actionBuf.len+ttSize;
    usint ttBaseOffsetNumBytes;
    byte* ttBaseOffsetLEB=encodeAsLEB128((byte*)&ttBaseOffsetGuess,sizeof(ttBaseOffsetGuess),false,&ttBaseOffsetNumBytes);
    //see if there's some wiggle room at the bottom
    bool foundRoom=false;
    for(int idx=ttBaseOffsetNumBytes-1;idx>=0;idx--)
    {
      if(((ttBaseOffsetLEB[idx] & 0x7f)!=0x7f && idx>0) ||
         ((ttBaseOffsetLEB[idx] & 0x70)!=0x70))
      {
        foundRoom=true;
        break;
      }
    }
    if(!foundRoom)
    {
      //add another byte to the LEB for wiggle room
      ttBaseOffsetLEB=realloc(ttBaseOffsetLEB,ttBaseOffsetNumBytes+1);
      ttBaseOffsetLEB[ttBaseOffsetNumBytes-1] |= 0x80;//to show that it is no
      //longer the MSB
      ttBaseOffsetLEB[ttBaseOffsetNumBytes]=0;
      ttBaseOffsetNumBytes++;
    }


    //this is the offset from the beginning of the section to the
    //beginning of the type table
    addr_t offsetStartToTTTop=buf->len-lsdaStartOffset+ttBaseOffsetNumBytes
      +1 //for call site format
      +callSiteLengthLEBNumBytes+callSiteBuf.len
      +actionBuf.len;
    int misalignment=(offsetStartToTTTop%4)?(4-(offsetStartToTTTop%4)):0;
    //type table must be 4-byte aligned
    offsetStartToTTTop+=misalignment;
    //to get ttBase, add the length of the type table buf and subtract
    //the number of bytes before the location we write the offset to
    //get a self-relative offset
    addr_t ttBaseOffset=offsetStartToTTTop+ttSize-
      (buf->len-lsdaStartOffset)-1;//-1 because points to last entry
                                   //-in table, not below it table it
                                   //-appears (no standard to confirm)
    usint ttBaseOffsetNumBytesNew;
    ttBaseOffsetLEB=encodeAsLEB128((byte*)&ttBaseOffset,sizeof(ttBaseOffset),false,
                                   &ttBaseOffsetNumBytesNew);
    if(ttBaseOffsetNumBytesNew < ttBaseOffsetNumBytes)
    {
      //we added in wiggle room but didn't actually have to use
      //it. We'll just waste a byte
      assert(ttBaseOffsetNumBytes-ttBaseOffsetNumBytesNew==1);
      ttBaseOffsetLEB=realloc(ttBaseOffsetLEB,ttBaseOffsetNumBytes);
      ttBaseOffsetLEB[ttBaseOffsetNumBytes-2] |= 0x80;//to show that it is no
      //longer the MSB
      ttBaseOffsetLEB[ttBaseOffsetNumBytes-1]=0;
    }

    if(lsda->ttEncoding!=DW_EH_PE_omit)
    {
      //now we can actually write everything in
      addToGrowingBuffer(buf,ttBaseOffsetLEB,ttBaseOffsetNumBytes);
    }
    byte callSiteFormat=DW_EH_PE_uleb128;
    addToGrowingBuffer(buf,&callSiteFormat,1);
    addToGrowingBuffer(buf,callSiteLengthLEB,callSiteLengthLEBNumBytes);
    addToGrowingBuffer(buf,callSiteBuf.data,callSiteBuf.len);
    free(callSiteBuf.data);
    addToGrowingBuffer(buf,actionBuf.data,actionBuf.len);
    free(actionBuf.data);
    if(lsda->ttEncoding!=DW_EH_PE_omit)
    {
      if(misalignment)
      {
        word_t zero=0;
        addToGrowingBuffer(buf,&zero,misalignment);
      }
      //////////////////////////////
      //write out the type table
      for(int j=lsda->numTypeEntries-1;j>=0;j--)
      {
        //it appears empirically that this is the format the type table
        //takes. Not really governed by any standard.
        addr_t pointerLocation=cfi->exceptTableAddress+buf->len+j*ttEntrySize;
        int numBytesOut;
        addr_t typeinfo=encodeEHPointerFromEncoding(lsda->typeTable[j],
                                                    lsda->ttEncoding,
                                                    pointerLocation,
                                                    &numBytesOut);
        assert(numBytesOut==ttEntrySize);
        addToGrowingBuffer(buf,&typeinfo,ttEntrySize);
      
      }
    } //end if(lsda->ttEncoding!=DW_EH_PE_omit)
    //todo: do we need more alignment at the end
  }//end loop over lsdas for writing
  
}

word_t readCallSiteTable(LSDA* lsda,byte* bytes,word_t offset,word_t callSiteTableSize,byte callSiteFormat,Elf_Data* data,addr_t sectionAddress)
{
  word_t oldOffset=offset;
  //read the call site table
  CallSiteRecord* callSite=NULL;
  int i=0;
  while(i<callSiteTableSize)
  {
    if(offset+i > data->d_size)
    {
      death("Malformed gcc_except_table\n");
    }
    //set up the call site we're dealing with right now
    lsda->numCallSites++;
    lsda->callSiteTable=realloc(lsda->callSiteTable,lsda->numCallSites*sizeof(CallSiteRecord));
    callSite=&(lsda->callSiteTable[lsda->numCallSites-1]);
    usint numBytesOut;
    callSite->position=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                       sectionAddress+offset+i,
                                       callSiteFormat,&numBytesOut);
    i+=numBytesOut;
    callSite->length=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                     sectionAddress+offset+i,
                                     callSiteFormat,&numBytesOut);
    i+=numBytesOut;
    callSite->landingPadPosition=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                                 sectionAddress+offset+i,
                                                 callSiteFormat,&numBytesOut);
    i+=numBytesOut;
    callSite->firstAction=decodeEHPointer(bytes+i,data->d_size-offset-i,
                                          sectionAddress+offset+i,
                                          callSiteFormat,&numBytesOut);
    i+=numBytesOut;
    //firstAction is not at all in the form we want. It's one plus a
    //byte offset into the action table. We want an index into the
    //action table we'll read. At the moment, however, we have no
    //way of calculating the index so we'll temporarily store the
    //wrong value in firstAction.
      
    callSite->hasAction=(callSite->firstAction!=0);
    callSite->firstAction-=1;//since it was biased by one so we
    //could tell if it was empty

    if(i>callSiteTableSize)
    {
      death("Call site table in .gcc_except_frame was not properly formatted\n");
    }
  }
  bytes+=i;
  offset+=i;
  if(offset>data->d_size)
  {
    death("Malformed LSDA in .gcc_except_table, ran out of space while reading the call site table\n");
  }
  return offset-oldOffset;
}

word_t readActionTable(LSDA* lsda,byte* bytes,word_t offset)
{
  word_t oldOffset=offset;

  //we keep track of the mapping between offsets into the action table
  //and the index assigned to the action. This is so that we don't
  //read actions multiple times. This array has one entry for every
  //entry in lsda->ActionTable
  addr_t* actionOffsets=NULL;

  addr_t highestActionOffset=0;//try to find the end of the action table
  for(int j=0;j<lsda->numCallSites;j++)
  {
    if(!lsda->callSiteTable[j].hasAction)
    {
      continue;
    }
    addr_t actionOffset=lsda->callSiteTable[j].firstAction;

    //previous action in the chain. -1 means invalid
    idx_t previousActionIdx=-1;

    //keep track so we don't set the call site for everything in the
    //chain, which would be wrong
    bool fixedFirstAction=false;

    bool continueChain=true;
    //follow the action chain for a single call site
    while(continueChain)
    {
      //see if we've read this action already
      bool foundAction=false;
      idx_t actionIdx=-1;
      for(int k=0;k<lsda->numActionEntries;k++)
      {
        if(actionOffsets[k]==actionOffset)
        {
          foundAction=true;
          continueChain=false;
          actionIdx=k;
          break;
        }
      }
      if(!foundAction)
      {
        //printf("action offset is 0x%zx\n",actionOffset);
        usint numBytesOut;
        idx_t typeIdx=leb128ToUWord(bytes+actionOffset,&numBytesOut);
        usint numBytesOut2;
        sword_t nextRecordPtr=leb128ToSWord(bytes+actionOffset+numBytesOut,&numBytesOut2);
        //printf("nextRecordPtr is %i\n",(int)nextRecordPtr);
        if(nextRecordPtr != 0)
        {
          //add numBytesOut to make it relative to the start of the action record
          nextRecordPtr+=numBytesOut;
        }
        highestActionOffset=max(highestActionOffset,actionOffset+numBytesOut+numBytesOut2);

        //create a new action record
        lsda->numActionEntries++;
        lsda->actionTable=realloc(lsda->actionTable,sizeof(ActionRecord)*lsda->numActionEntries);
        MALLOC_CHECK(lsda->actionTable);
        actionIdx=lsda->numActionEntries-1;
        memset(lsda->actionTable+actionIdx,0,sizeof(ActionRecord));

        //update teh action offsets table too
        actionOffsets=realloc(actionOffsets,sizeof(addr_t)*lsda->numActionEntries);
        MALLOC_CHECK(actionOffsets);
        actionOffsets[actionIdx]=actionOffset;
        
        //type indices seem to be 1-based, we make them 0-based here
        //being 0 in the first place means match all types
        if(typeIdx==0)
        {
          lsda->actionTable[actionIdx].typeFilterIndex=TYPE_IDX_MATCH_ALL;
        }
        else
        {
          assert(typeIdx >= 1);
          lsda->actionTable[actionIdx].typeFilterIndex=typeIdx-1;
        }
        if(nextRecordPtr==0)
        {
          //reached the end of this chain
          lsda->actionTable[actionIdx].hasNextAction=false;
          continueChain=false;
        }
        else
        {
          lsda->actionTable[actionIdx].hasNextAction=true;
        }
        actionOffset=(sword_t)actionOffset+nextRecordPtr;

      }
      if(!fixedFirstAction)
      {
        //make the first action field into the correct form
        lsda->callSiteTable[j].firstAction=actionIdx;
        fixedFirstAction=true;//otherwise we keep setting it over
        //again for everything in the chain,
        //which is wrong
      }
      
      //can't set nextAction yet as we don't know the index of the
      //next one. Can set the nextAction field for the previous action
      //in the chain, however.
      if(previousActionIdx!=-1)
      {
        lsda->actionTable[previousActionIdx].nextAction=actionIdx;
      }
      previousActionIdx=actionIdx;
    }//end following the action chain for a specific call stie
  }
  bytes+=highestActionOffset;
  offset+=highestActionOffset;
  return offset-oldOffset;
}

word_t readTypeTable(LSDA* lsda,byte* bytes,word_t offset,addr_t ttBase,
                   addr_t sectionAddress)
{
  word_t oldOffset=offset;
  //Read the type table
  //remember the type table grows from larger offsets to smaller
  //offsets, i.e. the smallest index is at the end

  if(lsda->ttEncoding!=DW_EH_PE_omit)
  {
    //todo: this won't work if LEB encoding used. If we start
    //observing gcc emitting LEB values in the type table we'll need
    //to switch to something else like only going through the type
    //table entries at offsets given in the action table
    int ttEntrySize=getPointerSizeFromEHPointerEncoding(lsda->ttEncoding);

    idx_t ttIdx=0;
    //printf("ttBase is 0x%zx\n",ttBase);
    //printf("ttEntrySize is 0x%x\n",ttEntrySize);
    //printf("offset is 0x%zx\n",offset);
    for(sword_t ttOffset=ttBase-ttEntrySize-offset;
        ttOffset >= 0;
        ttOffset-=ttEntrySize,ttIdx++)
    {
      //printf("ttOffset is 0x%zx\n",ttOffset);
      lsda->numTypeEntries++;
      lsda->typeTable=realloc(lsda->typeTable,sizeof(word_t)*lsda->numTypeEntries);
      usint numBytesOut;
      lsda->typeTable[ttIdx]=decodeEHPointer(bytes+ttOffset,ttEntrySize,
                                             sectionAddress+offset+ttOffset,
                                             lsda->ttEncoding,&numBytesOut);
    }
    bytes+=ttBase-offset;
    offset=ttBase;
  }
  return offset-oldOffset;
}

//builds an ExceptTable object from the raw ELF section note that the
//format of this section is not governed by any standard, so there is
//no means to validate that what we are doing is correct or even that
//it will work with all binaries emitted by gcc.
//We read all LSDAs specified in the lsdaPointers list and ignore all
//other data in the section. This is in keeping with what
//libstdc++/libgcc do.
ExceptTable parseExceptFrame(Elf_Scn* scn,addr_t* lsdaPointers,int numLSDAPointers)
{
  ExceptTable result;
  memset(&result,0,sizeof(result));
  Elf_Data* data=elf_getdata(scn,NULL);
  if(data->d_size == 0)
  {
    return result;
  }

  GElf_Shdr shdr;
  getShdr(scn,&shdr);
  addr_t sectionAddress=shdr.sh_addr;
  
  word_t offset=0;
  for(int lsdaPtrIdx=0;lsdaPtrIdx<numLSDAPointers;lsdaPtrIdx++)
  {
    offset=lsdaPointers[lsdaPtrIdx] - sectionAddress;
    if(offset < 0 || offset > data->d_size)
    {
      death("Failed to parse except table, request for LSDA pointer outside allowable range\n");
    }
    byte* bytes=data->d_buf+offset;
    
    //increase the number of LSDAs
    result.numLSDAs++;
    result.lsdas=realloc(result.lsdas,result.numLSDAs*sizeof(LSDA));
    LSDA* lsda=&(result.lsdas[result.numLSDAs-1]);
    memset(lsda,0,sizeof(LSDA));
    
    //start by reading the header
    byte lpStartFormat=bytes[0];
    bytes++;
    offset++;
    usint numBytesOut;
    if(lpStartFormat!=DW_EH_PE_omit)
    {
      lsda->lpStart=decodeEHPointer(bytes,data->d_size-offset,sectionAddress+offset,lpStartFormat,&numBytesOut);
      bytes+=numBytesOut;
      offset+=numBytesOut;
    }
    lsda->ttEncoding=bytes[0];
    bytes++;
    offset++;
    usint numSeptets;
    addr_t ttBase=0;
    if(lsda->ttEncoding!=DW_EH_PE_omit)
    {
      byte* ttBaseBytes=decodeLEB128(bytes,false,&numBytesOut,&numSeptets);
      assert(sizeof(ttBase) >= numBytesOut);
      memcpy(&ttBase,ttBaseBytes,numBytesOut);
      free(ttBaseBytes);
      //remember ttBase is a self-relative offset to the end of the type
      //table. For our purposes it's helpful if it is instead relative
      //to the beginning of this section
      ttBase+=offset;
      //ttBase seems to be off by one in what gcc emits. I am not
      //positive whether this is due to it pointing to the last byte of
      //the table rather than being below the table or if it's because
      //it's supposed to be relative to the end of ttBaseBytes. If the
      //later, then this offset is wrong and we will notice a problem
      //later. As before, there is no standard governing this section
      ttBase+=1;
      bytes+=numSeptets;
      offset+=numSeptets;
    }
    byte callSiteFormat=bytes[0];
    bytes++;
    offset++;
    word_t callSiteTableSize=decodeEHPointer(bytes,data->d_size-offset,
                                             sectionAddress+offset,
                                             callSiteFormat,&numBytesOut);
    bytes+=numBytesOut;
    offset+=numBytesOut;

    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table, ran out of space while reading the header\n");
    }
    
    word_t bytesRead=readCallSiteTable(lsda,bytes,offset,callSiteTableSize,
                                       callSiteFormat,data,sectionAddress);
    bytes+=bytesRead;
    offset+=bytesRead;

    //read the action table and type table. Now here's the icky part:
    //there is no really easy way to tell where the action table ends
    //and the type table begins. The way gcc is designed, there's no
    //need to ever parse this section directly. Access to the action
    //table is made only based on information in the call site table,
    //and access to the type table is made only based on information
    //in the action table. Therefore, the most reasonable way for us
    //to parse it is probably by following the same logic. Note that
    //we do not optimize, so we may end up parsing the same entry more
    //than once. Big deal.
    
    bytesRead=readActionTable(lsda,bytes,offset);
    bytes+=bytesRead;
    offset+=bytesRead;
    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table, ran out of space while reading the action table\n");
    }
    bytesRead=readTypeTable(lsda,bytes,offset,ttBase,sectionAddress);
    bytes+=bytesRead;
    offset+=bytesRead;
    if(offset>data->d_size)
    {
      death("Malformed LSDA in .gcc_except_table\n");
    }

  }
  return result;
}

