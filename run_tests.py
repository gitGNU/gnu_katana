#!/usr/bin/env python
#File: run_tests.py
#Author: James Oakley
#Copyright (C): 2010 Dartmouth College
#License: GNU General Public License
#Date: January, 2010
#Description: runs unit tests and reports their success or failure

import subprocess,sys,string,os,os.path
from optparse import OptionParser
passTests=0
totalTests=0

if os.path.exists("katana_log"):
  os.remove("katana_log")
if os.path.exists("katana_err_log"):
  os.remove("katana_err_log")

def runTestInDir(dir,execName="test"):
  msg="running test "+os.path.basename(dir)
  dots=string.join(['.' for x in range(0,35-len(msg))],'')
  sys.stdout.write(msg+dots+'|')
  sys.stdout.flush()
  if 0==subprocess.call(["python","validator.py",dir,execName]):
    sys.stdout.write("PASSED\n")
    return True
  else:
    sys.stdout.write("FAILED\n")
    return False

def runTestsInDir(dirName,execName):
  passTests=0
  totalTests=0
  for dir in sorted(os.listdir(dirName)):
    if os.path.isdir(os.path.join(dirName,dir)):
      if dir.startswith("t"):
        if True==runTestInDir(os.path.join(dirName,dir)):
          passTests+=1
        totalTests+=1
  return (passTests,totalTests)

  
parser=OptionParser()
parser.add_option("-d", action="store_true", dest="runDirectory", default=False, help="Take the following argument as directories of tests rather than as a single test")
(options,args)=parser.parse_args()
if len(args)>2:
  sys.stderr.write("Too many arguments\n")
  sys.exit(1)
  
if len(args)>0:
  if len(args)>1:
    execName=args[1]
  else:
    execName="test"
  if options.runDirectory:
    (passTests,totalTests)=runTestsInDir(args[0],execName)
  else:
    if True==runTestInDir(args[0],execName):
      passTests+=1
    totalTests+=1
else:
  #run the special tests, for large, real programs
  (passTests,totalTests)=runTestsInDir("tests","test")
  if True==runTestInDir("real_tests/apache","httpd"):
    passTests+=1
  totalTests+=1

print "Passed "+str(passTests)+" out of "+str(totalTests)+" tests"
if passTests!=totalTests:
  print "See validator_log for more information"
  sys.exit(1)

