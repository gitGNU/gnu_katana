import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test3\n");
    return False

  regexValues="alpha: (42),\(foo: (111,128)\),(69)"
  regexNewFields="v1: foo\.field2 is (0) and field3 is (0)"
  linecount=0
  patchingHappened=False
  for line in f:
    linecount+=1
    pattern="(no pattern chosen)"
    def patternError():
      sys.stderr.write("Line "+str(linecount)+" did not match  pattern\n")
      sys.stderr.write("Offending line is:\n")
      sys.stderr.write(line)
      sys.stderr.write("and the pattern (regex) is:\n")
      sys.stderr.write(pattern)
      sys.stderr.write("\n")
      if patchingHappened:
        sys.stderr.write("Note, patching has been done")
    if line.startswith("has pid"):
      continue
    if line.startswith("alpha"):
      pattern=regexValues
    elif line.startswith("v1: foo"):
      pattern=regexNewFields
      patchingHappened=True
    elif line.startswith("v0: field") or line.startswith("v1: field"):
      #technically we could do some checking on the addresses printed out here
      #to make sure they make sense, but it's probably not worth it
      continue
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
