#File: validator.py
#Author: James Oakley
#Copyright (C): 2010 Dartmouth College
#License: GNU General Public License
#Date, January, 2010
#Description: validate the output of a single unit test for katana

import subprocess,sys,os,os.path,time,string

proc=None

def cleanup():
  hotlogf.close()
  hotlogerrf.close()
  vlogf.close()
  if proc:
    proc.terminate() #kill it if it is still active
  sys.stdout.flush()

vlogf=open("validator_log","a")

if len(sys.argv)<2:
  print "Wrong number of arguments"
  print "Usage: "+sys.argv[0]+" DIRECTORY_TO_TEST [EXECUTABLE_NAME]"
  print "Default executable name is 'test'"
  sys.exit(1)

sys.path.append(os.path.abspath(sys.argv[1]))
validateModule=__import__("validate")

vlogf.write("############################\n")
vlogf.write("Validator running in dir: "+sys.argv[1]+"\n")
oldTree=os.path.join(sys.argv[1],"v0")
newTree=os.path.join(sys.argv[1],"v1")
execName="test"
if len(sys.argv)>2:
  execName=sys.argv[2]
logfname=os.path.join(sys.argv[1],"log")
logf=open(logfname,"w")
logf.truncate()

#generate the patch
klogfname=os.path.join(sys.argv[1],"katana_log")
hotlogf=open(klogfname,"w")
klogerrfname=os.path.join(sys.argv[1],"katana_err_log")
hotlogerrf=open(klogerrfname,"w")
hotlogf.write("\nStarting patch of "+os.path.join(oldTree,execName)+"\n-------------------------------------------\n")
hotlogf.flush()
patchOut=os.sys.argv[1]+".po"
args=["./katana","-g","-o",patchOut,oldTree,newTree,execName]
vlogf.write("running:\n "+string.join(args," ")+"\n")
kproc=subprocess.Popen(args,stdout=hotlogf,stderr=hotlogerrf)
if 0!=kproc.wait():
  vlogf.write("Validator failed because katana exited with failure while generating patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  space=string.join([' ' for x in range(0,27)],'')
  sys.stdout.write(space)
  cleanup()
  sys.exit(1)


sys.stdout.write("...gen...|")
sys.stdout.flush()

proc=subprocess.Popen([os.path.join(oldTree,execName)],stdout=logf)
#sleep for a moment to let the process run
time.sleep(0.5)
#now start the hotpatcher
args=["./katana","-p",patchOut,str(proc.pid)]
vlogf.write("running "+string.join(args," ")+"\n")
kproc=subprocess.Popen(args,stdout=hotlogf,stderr=hotlogerrf)

if 0!=kproc.wait():
  vlogf.write("Validator failed because katana exited with failure while applying patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  space=string.join([' ' for x in range(0,17)],'')
  sys.stdout.write(space)
  cleanup()
  sys.exit(1)

sys.stdout.write("...patch...|")
sys.stdout.flush()
  
hotlogf.close()
#sleep for a moment to let the process print out new values now that
#it's been patched
time.sleep(0.5)
if proc.poll():
  logf.close()
  vlogf.write("Validator failed because the target program crashed\n")
  vlogf.close()
  space=string.join([' ' for x in range(0,5)],'')
  sys.stdout.write(space)
  sys.exit(1)
proc.terminate() #kill it
dots=string.join(['.' for x in range(0,5)],'')
sys.stdout.write(dots)
sys.stdout.flush()
oldstdout=sys.stdout
oldstderr=sys.stderr
sys.stdout=vlogf
sys.stderr=vlogf
result=validateModule.validate(logfname)
sys.stdout=oldstdout
sys.stderr=oldstderr
if not result:
  logf.close()
  vlogf.write("Validator failed because output from target program was unexpected\n")
  vlogf.close()
  sys.exit(1)

logf.close()

