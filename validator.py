#File: validator.py
#Author: James Oakley
#Copyright (C): 2010 Dartmouth College
#License: GNU General Public License
#Date, January, 2010
#Description: validate the output of a single unit test for katana

import subprocess,sys,os,os.path,time,string
from optparse import OptionParser

proc=None

def cleanup():
  hotlogf.close()
  hotlogerrf.close()
  vlogf.close()
  if proc:
    if options.superuser:
      subprocess.call(['sudo','kill',str(proc.pid)])
    else:
      proc.terminate() #kill it if it is still active
  sys.stdout.flush()

vlogf=open("validator_log","a")

parser=OptionParser()
parser.add_option("-s", action="store_true", dest="superuser", default=False, help="Specifies that the program must be run as root")
(options,args)=parser.parse_args()


if len(args)<1:
  print("Wrong number of arguments")
  print("Usage: "+sys.argv[0]+" DIRECTORY_TO_TEST [EXECUTABLE_NAME]")
  print("Default executable name is 'test'")
  sys.exit(1)

if options.superuser:
  subprocess.call(['sudo','true']) #the actual command will be run separately from this process, this one will not block on it, so make a blocking call first to get the sudo password cached
  
sys.path.append(os.path.abspath(args[0]))
validateModule=__import__("validate")

vlogf.write("############################\n")
vlogf.write("Validator running in dir: "+args[0]+"\n")
oldTree=os.path.join(args[0],"v0")
newTree=os.path.join(args[0],"v1")
execName="test"
if len(args)>1:
  execName=args[1]
programArguments=[]
if len(args)>2:
  programArguments=list(map(lambda x:x[1:] if x.startswith("/-") else x,args[2:]))
  for i in range(0,len(programArguments)):
    #unescape options for the executable
    if programArguments[i][0]=='\\':
      programArguments[i]=programArguments[i][1:]
logfname=os.path.join(args[0],"log")
logf=open(logfname,"w")
logf.truncate()

#generate the patch
klogfname=os.path.join(args[0],"katana_log")
hotlogf=open(klogfname,"w")
klogerrfname=os.path.join(args[0],"katana_err_log")
hotlogerrf=open(klogerrfname,"w")
hotlogf.write("\nStarting patch of "+os.path.join(oldTree,execName)+"\n-------------------------------------------\n")
hotlogf.flush()
patchOut=args[0]+".po"
args=["./katana","-g","-o",patchOut,oldTree,newTree,execName]
vlogf.write("running:\n "+" ".join(args)+"\n")
kproc=subprocess.Popen(args,stdout=hotlogf,stderr=hotlogerrf)
if 0!=kproc.wait():
  vlogf.write("Validator failed because katana exited with failure while generating patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  space=''.join([' ' for x in range(0,27)])
  sys.stdout.write(space)
  cleanup()
  sys.exit(1)


sys.stdout.write("...gen...|")
sys.stdout.flush()

procCmd=[];
if options.superuser:
  procCmd=['sudo']
procCmd.extend([os.path.join(oldTree,execName)])
if(len(programArguments)):
  procCmd.extend(programArguments)

vlogf.write("running\n " + " ".join(procCmd) + "\n")
proc=subprocess.Popen(procCmd,stdout=logf)
#sleep for a moment to let the process run
time.sleep(0.5)
#now start the hotpatcher
args=["./katana","-p",patchOut,str(proc.pid)]
vlogf.write("running\n " + " ".join(args) + "\n")
kproc=subprocess.Popen(args,stdout=hotlogf,stderr=hotlogerrf)

if 0!=kproc.wait():
  vlogf.write("Validator failed because katana exited with failure while applying patch.\nSee "+klogfname+" and "+klogerrfname+" for more information\n")
  space=''.join([' ' for x in range(0,17)])
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
  space=''.join([' ' for x in range(0,5)])
  sys.stdout.write(space)
  sys.exit(1)
proc.terminate() #kill it
dots=''.join(['.' for x in range(0,5)])
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

