#!/usr/bin/env python
#coding: utf-8

import lldb
import commands
import optparse
import shlex
import subprocess

# Before use this script, call below
# (lldb) settings set target.input-path .stdin.txt
# Use this like
# (lldb) command script import ./debug.py
# Add break points
# (lldb) addbp
# See stdin
# (lldb) stdin pram
# Set pram to stdin (./stdin.txt)
# (lldb) stdin
# Debug avoid stdin bug on MacOS
# (lldb) db

def ls(debugger, command, result, internal_dict): 
  if command is "":
    subprocess.call(['/bin/ls',"."])
  else:
    subprocess.call(['/bin/ls',command])

def cat(debugger, command, result, internal_dict):
  subprocess.call(['/bin/cat', command])

def stdin(debugger, command, result, internal_dict):
  if command is "":
    print "%s" % commands.getoutput('/bin/cat ./.stdin.txt')
  else:
    with open('.stdin.txt', 'w') as f:
      subprocess.call(['/bin/echo', command], stdout=f)
    print '"%s" > stdin' % command;  

def db(debugger, command, result, internal_dict):
  debugger.HandleCommand('run -i ".stdin.txt"')

def __lldb_init_module(debugger, internal_dict):
    debugger.HandleCommand("command script add -f debug.ls ls")
    debugger.HandleCommand("command script add -f debug.echo echo")    
    debugger.HandleCommand("command script add -f debug.stdin stdin")
    debugger.HandleCommand("command script add -f debug.cat cat")
    debugger.HandleCommand("command script add -f debug.db db")
    print "ls, cat and db has been installed."