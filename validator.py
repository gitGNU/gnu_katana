#File: validator.py
#Author: James Oakley
#Date, January, 2010
#Description: validate the output of a single unit test for katana

import subprocess,sys,os,os.path,time,string

vlogf=open("validator_log","a")

if len(sys.argv)!=2:
  print "Wrong number of arguments"
  print "Usage: "+sys.argv[0]+" DIRECTORY_TO_TEST"
  sys.exit(1)

sys.path.append(os.path.abspath(sys.argv[1]))
validateModule=__import__("validate")

vlogf.write("############################\n")
vlogf.write("Validator running in dir: "+sys.argv[1]+"\n")
oldTree=os.path.join(sys.argv[1],"v0")
newTree=os.path.join(sys.argv[1],"v1")
execName="test"
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
  hotlogf.close()
  hotlogerrf.close()
  vlogf.write("Validator failed because katana exited with failure while generating patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  vlogf.close()
  sys.exit(1)

proc=subprocess.Popen([os.path.join(oldTree,execName)],stdout=logf)
#sleep for a moment to let the process run
time.sleep(0.5)
#now start the hotpatcher
args=["./katana","-p",patchOut,str(proc.pid)]
vlogf.write("running "+string.join(args," ")+"\n")
kproc=subprocess.Popen(args,stdout=hotlogf,stderr=hotlogerrf)

if 0!=kproc.wait():
  hotlogf.close()
  hotlogerrf.close()
  vlogf.write("Validator failed because katana exited with failure while applying patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  proc.terminate() #kill it
  vlogf.close()
  sys.exit(1)
hotlogf.close()
#sleep for a moment to let the process print out new values now that
#it's been patched
time.sleep(0.5)
proc.terminate() #kill it
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

