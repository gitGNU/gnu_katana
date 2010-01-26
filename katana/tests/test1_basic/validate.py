import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test1\n")
    return False
  linecount=0
  regexOdd="field 1 at addr: ([a-f0-9]{4,8}), field 2 at addr: ([a-f0-9]{4,8})"
  regexEven="Foo: (42),(66)"
  addr1=None
  addr2=None
  addr1New=None
  addr2New=None
  field1=None
  field2=None
  for line in f:
    linecount+=1
    if linecount<3:
      continue
    if linecount%2:
      match=re.match(regexOdd,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected odd pattern\n")
        sys.stderr.write("Offending line is:\n")
        sys.stderr.write(line)
        return False
      if not addr1:
        addr1=match.group(1)
        addr2=match.group(2)
      elif (not addr1New and addr1!=match.group(1)) or (not addr2New and addr2!=match.group(2)):
        addr1New=match.group(1)
        addr2New=match.group(2)
      elif (addr1!=match.group(1) and addr1New!=match.group(1)) or (addr2!=match.group(2) and addr2New!=match.group(2)):
        sys.stderr.write("addr1 or addr2 is wrong, changed for a second time\n");
        sys.stderr.write("Offending line is:\n");
        sys.stderr.write(line)

        return False
    else:
      match=re.search(regexEven,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected even pattern\n")
        sys.stderr.write("Offending line is:\n")
        sys.stderr.write(line)
        return False
      if not field1:
        field1=match.group(1)
        field2=match.group(2)
      elif field1!=match.group(1) or field2!=match.group(2):
        sys.stderr.write("Field 1 or Field2 changed, this should not occur\n");
        sys.stderr.write("Offending line is:\n");
        sys.stderr.write(line)
        return False
  
  if not addr1New or not addr2New:
    sys.stderr.write("It appears that hotpatching never happened\n")
    return False
  return True
      

if __name__ == "__main__":
  validate(sys.argv[1])
