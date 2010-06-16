/*
  File: path.h
  Author: James Oakley
  Copyright (C): 2010 Dartmouth College
  License: GNU General Public License
  Project:  katana
  Date: February 2010
  Description: Functions for dealing with filepaths
*/

#ifndef path_h
#define path_h
#include <stdbool.h>

char* joinPaths(char* path1,char* path2);

//path is made relative to ref
char* makePathRelativeTo(char* path,char* ref);

//return the directory portion of path
char* getDirectoryOfPath(char* path);

//The returned string should be freed
char* absPath(char* path);
bool isAbsPath(char* path);
#endif
