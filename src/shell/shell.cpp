/*
  File: shell.cpp
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
  Date: December 2010
  Description: The katana shell
*/

#include "shell.h"
#include "parse_helper.h"
#include "shell/shell.yy.h"

extern "C"
{
#include "util/dictionary.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>
}

extern int yydebug;
extern FILE *yyin;
extern int yyparse();
extern ParseNode rootParseNode;

//dictionary mapping variable names to the ShellVariable objects themselves
//note that shell.y makes use of this
Dictionary* shellVariables=NULL;



//this snippet adapted from the GNU Readline Library documentation
/* Read a string, and return a pointer to it.
   Returns NULL on EOF. */
char* rlgets(char* prompt,bool isInputTty)
{
  //A static variable for holding the line.
  static char *lineRead = (char *)NULL;
  //If the buffer has already been allocated,
  //return the memory to the free pool.
  if(lineRead)
  {
    free(lineRead);
    lineRead = NULL;
  }

  if(isInputTty)
  {
    //Get a line from the user.
    lineRead = readline(prompt);
    /* If the line has any text in it,
       save it on the history. */
    if(lineRead && *lineRead)
    {
      add_history (lineRead);
    }
    return lineRead;
  }
  else
  {
    //input is not a tty so we don't care about readline and indeed
    //don't want readline because readline echoes the input. So we
    //just use fgets assuming 2048 will be enough space. todo: write
    //something variable or at least do good error checking
    lineRead=(char*)malloc(2048);
    return fgets(lineRead,2048,stdin);
    
  }
}



void doShell(char* inputFilename)
{
  shellVariables=dictCreate(100);//todo: sensible number not arbitrary number 100
  yydebug=1;
  if(!inputFilename)
  {
    //in interactive or pipe mode, reading from stdin
    char* line;
    char* prompt=(char*)"> ";
    bool isInputTty=isatty(fileno(stdin));
    while((line=rlgets(prompt,isInputTty)))
    {
      yy_scan_string(line);
      bool parseSuccess = (0==yyparse());
      if(!parseSuccess)
      {
        printf("Unable to parse your input. Try again\n");
        continue;
      }
      assert(rootParseNode.type==PNT_LIST);
      CommandList* list=rootParseNode.u.listItem;
      while(list)
      {
        list->cmd->execute();
        CommandList* next=list->next;
        free(list);
        list=next;
      }
    }
  }
  else
  {
    FILE* file=fopen(inputFilename,"r");
    if(!file)
    {
      logprintf(ELL_ERR,ELS_SHELL,"Cannot open shell input file %s\n",inputFilename);
      return;
    }
    yyin=file;
    bool parseSuccess = (0==yyparse());
    if(!parseSuccess)
    {
      printf("Unable to parse file %s\n",inputFilename);
      return;
    }
    fclose(file);
    assert(rootParseNode.type==PNT_LIST);
    CommandList* list=rootParseNode.u.listItem;
    while(list)
    {
      assert(list->cmd);
      list->cmd->execute();
      CommandList* next=list->next;
      free(list);
      list=next;
    }
  }
  dictDelete(shellVariables,ShellVariable::deleteShellVariable);
}
