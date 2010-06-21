#File: validate.py
#Author: James Oakley
#Copyright (C): 2010 Dartmouth College
#License: GNU General Public License
#Date, February, 2010
#Description: validate the output of a single unit test for katana


import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test3\n");
    return False


  regexV0="v0: I do nothing";
  regexV1Alpha="v1: alpha is {(42),(66)}";
  regexV1Beta="v1: beta is (24)";
  linecount=0
  patchingHappened=False
  for line in f:
    linecount+=1
    patern="(no pattern chosen)"
    def patternError():
      sys.stderr.write("Line "+str(linecount)+" did not match  pattern\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
      sys.stderr.write("and the pattern (regex) is:\n")
      sys.stderr.write(pattern)
      sys.stderr.write("\n")
      if patchingHappened:
        sys.stderr.write("Note, patching has been done\n")
    if line.startswith("has pid"):
      continue;
    elif line.startswith("v0"):
      pattern=regexV0
    elif line.startswith("v1: alpha"):
      pattern=regexV1Alpha
      patchingHappened=True
    elif line.startswith("v1: beta"):
      pattern=regexV1Beta
      patchingHappened=True
    else:
      pattern="line doesn't even start like any pattern"
      patternError()
    match=re.search(pattern,line)
    if not match:
      patternError()
      return False
  if not patchingHappened:
    sys.stderr.write("It appears patching never happened\n")
    return False
  return True
