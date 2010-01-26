import sys,re
def validate(logname):
  f=open(logname)
  if not f:
    sys.stderr.write("Cannot open log file for test1\n")
    return False
  linecount=0
  regexMod2="Foo: (42),(66)"
  regexMod3="field 1 at addr: ([a-f0-9]{4,8}), field 2 at addr: ([a-f0-9]{4,8})"
  regexMod0="\(Other\) Foo: (31),(13)"
  regexMod1="\(Other\) field 1 at addr: ([a-f0-9]{4,8}), field 2 at addr: ([a-f0-9]{4,8})"
  addr1=None
  addr2=None
  addr1New=None
  addr2New=None
  field1=None
  field2=None
  addr1Other=None
  addr2Other=None
  addr1NewOther=None
  addr2NewOther=None
  field1Other=None
  field2Other=None
  for line in f:
    linecount+=1
    if linecount<3:
      continue
    if 2==linecount%4:
      match=re.search(regexMod2,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected Mod2 pattern\n")
        sys.stderr.write("Offending line is:\n")
        sys.stderr.write(line)
        return False
      if not field1:
        field1=match.group(1)
        field2=match.group(2)
      elif field1!=match.group(1) or field2!=match.group(2):
        return False
    elif 3==linecount%4:
      match=re.match(regexMod3,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected Mod3 pattern\n")
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
    if 0==linecount%4:
      match=re.match(regexMod0,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected Mod0 pattern\n")
        sys.stderr.write("Offending line is:\n")
        sys.stderr.write(line)
        return False
      if not field1Other:
        field1Other=match.group(1)
        field2Other=match.group(2)
      elif field1Other!=match.group(1) or field2Other!=match.group(2):
        return False
    elif 1==linecount%4:
      match=re.match(regexMod1,line)
      if not match:
        sys.stderr.write("Line "+str(linecount)+" did not match expected Mod1 pattern\n")
        sys.stderr.write("Offending line is:\n")
        sys.stderr.write(line)
        return False
      if not addr1Other:
        addr1Other=match.group(1)
        addr2Other=match.group(2)
      elif (not addr1NewOther and addr1Other!=match.group(1)) or (not addr2NewOther and addr2Other!=match.group(2)):
        addr1NewOther=match.group(1)
        addr2NewOther=match.group(2)
      elif (addr1Other!=match.group(1) and addr1NewOther!=match.group(1)) or (addr2Other!=match.group(2) and addr2NewOther!=match.group(2)):
        sys.stderr.write("addr1 or addr2 is wrong, changed for a second time\n");
        sys.stderr.write("Offending line is:\n");
        sys.stderr.write(line)
        return False
      
  
  if not addr1New or not addr2New:
    sys.stderr.write("It appears that hotpatching in main.c never happened\n")
    return False
  if not addr1NewOther or not addr2NewOther:
    sys.stderr.write("It appears that hotpatching in other.c never happened\n")
    return False
  return True
      

if __name__ == "__main__":
  validate(sys.argv[1])
