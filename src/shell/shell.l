%option noyywrap
%option nounput
%option noinput
%x STAR_COMMENT
%x SLASH_COMMENT
%x POUND_COMMENT
%x IN_STRING
%x IN_SHELL_CMD
%option header-file="shell/shell.yy.h"
%option outfile="shell/shell.yy.cpp"
   
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

}

VARIABLE         \$[_a-zA-Z][_a-zA-Z0-9]*
INT       [0-9]+
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
"data"    return T_DATA;
"dwarfscript" return T_DWARFSCRIPT;
"compile" return T_COMPILE;
"emit" return T_EMIT;

{VARIABLE}   savedVarName=strdup(yytext);return T_VARIABLE;


<IN_STRING>{
"\""        savedString=strdup(finishAccumulateText());BEGIN(INITIAL);return T_STRING_LITERAL;
[^\"\n\\]*  accumulateText(yytext);
\\\n        lineNumber++;//eat newlines
\\\"        accumulateText(yytext);
\\          accumulateText(yytext);
}

<IN_SHELL_CMD>{
  [^\n]*  accumulateText(yytext);
  [\n]    lineNumber++;savedString=strdup(finishAccumulateText());BEGIN(INITIAL);return T_STRING_LITERAL;
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

\n  lineNumber++;
. return T_INVALID_TOKEN;
%%

typedef struct
{
  const char* keyword;
  const int token;
} KeywordInfo;

#define NUM_C48_KEYWORDS 18

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
