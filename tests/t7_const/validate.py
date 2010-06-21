#File: validate.py
#Author: James Oakley
#Copyright (C): 2010 Dartmouth College
#License: GNU General Public License
#Date, March, 2010
#Description: validate the output of a single unit test for katana


import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test3\n");
    return False


  regexAlpha="alpha: ([0-9]*)"
  regexBeta="beta: ([0-9]*),([0-9]*),([0-9]*)"

  
  linecount=0
  global alphaChanged,betaChanged #I don't get why this is necessary,
                                  #should be closure, I don't
                                  #understand python well enough
  alphaChanged=False
  betaChanged=False

  def validateAlpha(match):
    global alphaChanged #I don't get why this is necessary,
                        #should be closure, I don't
                        #understand python well enough
    if alphaChanged==True:
      if match.group(1)!="3":
        sys.stderr.write("alpha should be 3, is "+match.group(1)+"\n");
        return False
    else:
      if match.group(1)=="3":
        alphaChanged=True
      elif match.group(1)!="2":
        sys.stderr.write("alpha should be 2, is "+match.group(1)+"\n");
        return False
    return True

  def validateBeta(match):
    global betaChanged
    if betaChanged:
      if match.group(1)!="45" and match.group(2)!="43" and match.group(3)!="46":
        sys.stderr.write("beta should be 45,43,46, is "+match.group(1)+","+match.group(2)+","+match.group(3)+"\n");
        return False
    else:
      if match.group(1)=="45":
        betaChanged=True
      if match.group(1)!="42" and match.group(2)!="43" and match.group(3)!="44":
        sys.stderr.write("beta should be 42,43,44, is "+match.group(1)+","+match.group(2)+","+match.group(3)+"\n");
        return False
    return True


  for line in f:
    validateFunc=None
    linecount+=1
    patern="(no pattern chosen)"
    def patternError():
      sys.stderr.write("Line "+str(linecount)+" did not match  pattern\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
      sys.stderr.write("and the pattern (regex) is:\n")
      sys.stderr.write(pattern)
      sys.stderr.write("\n")
      if addressChanged['alpha']:
        sys.stderr.write("Note, patching has been done\n")
    def validationError():
      sys.stderr.write("Line "+str(linecount)+" did not pass validation\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
    if line.startswith("has pid"):
      continue
    elif line.startswith("alpha:"):
      pattern=regexAlpha
      validateFunc=validateAlpha
    elif line.startswith("beta:"):
      pattern=regexBeta
      validateFunc=validateBeta
    else:
      pattern="line doesn't even start like any pattern"
      patternError()
    match=re.search(pattern,line)
    if not match:
      patternError()
      return False
    if validateFunc:
      if not validateFunc(match):
        validationError()
        return False
  if not betaChanged:
    sys.stderr.write("It appears patching of beta never happened\n")
    return False
  if not alphaChanged:
    sys.stderr.write("It appears patching of alpha never happened\n")
    return False
  return True
