%option noyywrap
%option nounput
%option noinput
%x STAR_COMMENT
%x SLASH_COMMENT
%x POUND_COMMENT
%x IN_STRING
%x IN_SHELL_CMD
%option header-file="shell.yy.h"
%option outfile="shell.yy.cpp"
   
%top{
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include "shell/shell.tab.hpp"
void accumulateText(char* text);
char* finishAccumulateText();
int keywordOrID();
extern int lineNumber;
extern char* savedVarName;
extern char* savedString;
extern int savedInt;
extern bool returnedEOFAlready;
}

VARIABLE         \$[_a-zA-Z0-9]+
NONNEG_INT [0-9]+
HEX        0[Xx][0-9a-fA-F]+
FLOAT     ([0-9]*\.[0-9]+)|([0-9]+\.[0-9]*)
D_QUOTE_STRING  [\"][^\"\n][\"]
S_QUOTE_STRING  [\'][^\'\n][\']

%%

"/*"        BEGIN(STAR_COMMENT);
"//"        BEGIN(SLASH_COMMENT);
"#"        BEGIN(POUND_COMMENT);
"\""        BEGIN(IN_STRING);
"!"         BEGIN(IN_SHELL_CMD); return T_SHELL_COMMAND;

"="   return '=';
"load" return T_LOAD;
"save" return T_SAVE;
"translate" return T_TRANSLATE;
"replace" return T_REPLACE;
"section" return T_SECTION;
"raw" return T_RAW;
"data"    return T_DATA;
"dwarfscript" return T_DWARFSCRIPT;
"compile" return T_COMPILE;
"emit" return T_EMIT;
"info" return T_INFO;
"eh" return T_EXCEPTION_HANDLING;
"hash" return T_HASH;
"elf" return T_ELF;
"patch" return T_PATCH;
"gen" return T_GENERATE;
"generate" return T_GENERATE;
"apply" return T_APPLY;
";" return ';';
"\[" return '[';
"\]" return ']';

{VARIABLE}   savedVarName=strdup(yytext);return T_VARIABLE;
{NONNEG_INT}   savedInt=atoi(yytext); return T_NONNEG_INT;
{HEX}   savedInt=strtol(yytext, NULL, 16); return T_NONNEG_INT;


<IN_STRING>{
"\""        savedString=strdup(finishAccumulateText());BEGIN(INITIAL);return T_STRING_LITERAL;
[^\"\n\\]*  accumulateText(yytext);
\\\n        lineNumber++;//eat newlines
\\\"        accumulateText(yytext);
\\          accumulateText(yytext);
}

<IN_SHELL_CMD>{
  [^\n]*  accumulateText(yytext);savedString=strdup(finishAccumulateText());BEGIN(INITIAL);return T_STRING_LITERAL;
 }

<STAR_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   // eat comment in chunks
"\n"      lineNumber++;  
"*"       // eat the lone star
}

<SLASH_COMMENT>{
[\n]      lineNumber++;BEGIN(INITIAL);
[^\n]+   // eat comment in chunks
}

<POUND_COMMENT>{
  [\n]      lineNumber++;BEGIN(INITIAL);
  [^\n]+   // eat comment in chunks
}

[ \t]+

\n  lineNumber++; return T_EOL;
<INITIAL><<EOF>> {
  if(!returnedEOFAlready)
  {
    returnedEOFAlready=true;
    return T_EOF;
  }
  else
  {
    yyterminate();
  }
 }
. return T_INVALID_TOKEN;
%%


static char* textAccumulator=NULL;
static bool textAccumulating=false;

void accumulateText(char* text)
{
    if (!textAccumulating)
    {
        if (textAccumulator)
        {
            free(textAccumulator);
            textAccumulator = NULL;
        }
        textAccumulating = true;
        textAccumulator = (char*)malloc(2 * strlen(text) + 1);
        strcpy(textAccumulator, text);
    }
    else
    {
        int accumLen = strlen(textAccumulator);
        int newLen = strlen(text);
        textAccumulator = (char*)realloc(textAccumulator, accumLen + newLen + 1);
        if (!textAccumulating)
        {
            abort();
        }
        strcat(textAccumulator, text);
    }
}

char* finishAccumulateText()
{
    textAccumulating = false;
    return textAccumulator;
}
