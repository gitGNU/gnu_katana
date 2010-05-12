import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test3\n");
    return False


  regexLives="(alpha|beta|gamma\.foo|delta\.foo1|delta\.foo2) lives at 0x([a-f0-9]*)"
  regexAlphaVal="alpha is 42"
  regexBetaLives="beta lives at 0x([a-f0-9]*)"
  regexBetaVal="beta: 43, 44"
  regexGammaIdx="gamma idx is 45"
  regexGammaFooVal="gamma\.foo: 43, 46"
  regexSizeof="sizeof\(Baz\) is ([0-9])"
  regexDeltaIdx="delta idx is 1"
  regexDeltaFoo1Val="delta\.foo1: NULL, 2"
  regexDeltaFoo2Val="delta\.foo2: NULL, 3"

  
  linecount=0
  addressChanged={}
  addressChanged['alpha']=False
  addressChanged['beta']=False
  addressChanged['gamma.foo']=False
  bazSizeChanged=False
  bazSize=None;
  addrs={}
  addrs['alpha']=None
  addrs['beta']=None
  addrs['gamma.foo']=None
  addrs['delta.foo1']=None
  addrs['delta.foo2']=None

  def validateAddr(match):
    var=match.group(1)
    if not addrs[var]:
      addrs[var]=match.group(2)
      return True
    if addrs[var]!=match.group(2):
      if not addressChanged[var]:
        addressChanged[var]=True
        addrs[var]=match.group(2)
      else:
        sys.stderr.write("Address for var "+var+" changed more than once\n");
        sys.stderr.write("Previous address was 0x"+addrs[var]+"\n")
        sys.stderr.write("And the new one is 0x"+match.group(2)+"\n");
        return False
    return True

  def validateSizeof(match):
    if bazSizeChanged:
      if match.group(1)!=bazSize:
        sys.stderr.write("sizeof(Baz) changed more than once\n")
        return False
    elif not bazSize:
      bazSize=match.group(1)
      if bazSize!="12":
        sys.stderr.write("Unexpected sizeof(Baz), expected 12\n");
        return False
    elif match.group(1)!=bazSize:
      bazSizeChanged=True
      bazSize=match.group(1)
      if bazSize!="20":
        sys.stderr.write("Unexpected sizeof(Baz), expected 20\n");
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
      if addressChanged['alpha']:
        sys.stderr.write("Note, patching has been done\n")
    def validationError():
      sys.stderr.write("Line "+str(linecount)+" did not pass validation\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
    if line.startswith("has pid"):
      continue
    elif line.startswith("beta lives") or line.startswith("gamma.foo lives") or line.startswith("delta.foo1 lives") or line.startswith("delta.foo2 lives"):
      pattern=regexLives
      validateFunc=validateAddr
    elif line.startswith("alpha is"):
      pattern=regexAlphaVal
    elif line.startswith("beta:"):
      pattern=regexBetaVal
    elif line.startswith("gamma idx"):
      pattern=regexGammaIdx
    elif line.startswith("gamma.foo:"):
      pattern=regexGammaFooVal
    elif line.startswith("delta idx"):
      pattern=regexDeltaIdx
    elif line.startswith("delta.foo1:"):
      pattern=regexDeltaFoo1Val
    elif line.startswith("delta.foo2:"):
      pattern=regexDeltaFoo2Val
    elif line.startswith("sizeof"):
      pattern=regexSizeof
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
  if not addressChanged['beta']:
    sys.stderr.write("It appears patching of beta never happened\n")
    return False
  if not addressChanged['gamma.foo']:
    sys.stderr.write("It appears patching of gamma never happened\n")
    return False
  if not bazSizeChanged:
    sys.stderr.write("sizeof(Baz) never changed\n")
    return False
  if not addrs['delta.foo1'] or not addrs['delta.foo2']:
    sys.stderr.write("It appears patching in of delta never happened\n")
    return False
  return True
