import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test3\n");
    return False


  regexAlphaLives="alpha lives at 0x([a-f0-9]*)"
  regexAlphaVal="alpha: 1, 2, 3"
  regexAlphaOtherLives="alpha\.other lives at (0x[a-f0-9]*)"
  regexAlphaOtherVal="alpha\.other: 33, 22, 11"
  regexAlphaOtherOther="alpha\.other has no other member"
  regexBetaLives="beta lives at 0x([a-f0-9]*)"
  regexBetaVal="beta: 33, 22, 11"
  regexBetaOther="beta has no other member"
  regexGammaLives="gamma lives at 0x([a-f0-9]*)"
  regexGammaVal="gamma: 44, 55, 66"
  regexGammaOtherLives="gamma\.other lives at 0x([a-f0-9]*)"
  regexGammaOtherVal="gamma\.other: 1, 2, 3"
  regexGammaOtherOtherLives="gamma\.other\.other lives at 0x([a-f0-9])"
  regexGammaOtherOtherVal="gamma\.other\.other: 33, 22, 11"
  regexGammaOtherOtherOther="gamma\.other\.other has no other member"

  
  linecount=0
  patchingHappened=[False,False,False] #one each for alpha, beta and gamma
  addrs=[None,None,None] #one entry each for alpha, beta, and gamma

  def validateAddr(var,match):
    if not addrs[var]:
      addrs[var]=match.group(1)
      return True
    if addrs[var]!=match.group(1):
      if not patchingHappened[var]:
        patchingHappened[var]=True
        addrs[var]=match.group(1)
      else:
        sys.stderr.write("Address for var "+var+" changed more than once\n");
        return False
    

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
      if patchingHappened:
        sys.stderr.write("Note, patching has been done\n")
    def validationError():
      sys.stderr.write("Line "+str(linecount)+" did not pass validation\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
    if line.startswith("has pid"):
      continue;
    elif line.startswith("alpha lives"):
      pattern=regexAlphaLives
    elif line.startswith("alpha:"):
      pattern=regexAlphaVal
    elif line.startswith("alpha.other lives"):
      pattern=regexAlphaOtherLives
    elif line.startswith("alpha.other:"):
      pattern=regexAlphaOtherVal
    elif line.startswith("alpha.other has"):
      pattern=regexAlphaOtherOther
    elif line.startswith("beta lives"):
      pattern=regexBetaLives
    elif line.startswith("beta:"):
      pattern=regexBetaVal
    elif line.startswith("beta has"):
      pattern=regexBetaOther
    elif line.startswith("gamma lives"):
      pattern=regexGammaLives
    elif line.startswith("gamma:"):
      pattern=regexGammaVal
    elif line.startswith("gamma.other lives"):
      pattern=regexGammaOtherLives
    elif line.startswith("gamma.other:"):
      pattern=regexGammaOtherVal
    elif line.startswith("gamma.other.other lives"):
      pattern=regexGammaOtherOtherLives
    elif line.startswith("gamma.other.other:"):
      pattern=regexGammaOtherOtherVal
    elif line.startswith("gamma.other.other has"):
      pattern=regexGammaOtherOtherOther
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
  if not patchingHappened[0]:
    sys.stderr.write("It appears patching of alpha never happened\n")
    return False
  if not patchingHappened[1]:
    sys.stderr.write("It appears patching of beta never happened\n")
    return False
  if not patchingHappened[2]:
    sys.stderr.write("It appears patching of gamma never happened\n")
    return False
  return True
