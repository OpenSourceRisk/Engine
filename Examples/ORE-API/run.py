#!/usr/bin/env python

import os, time, subprocess

print("+-----------------------------------------------------+")
print("| ORE-API                                             |")
print("+-----------------------------------------------------+")

proc1 = subprocess.Popen(['python3 simplefileserver.py'], shell=True)
time.sleep(5)

proc2 = subprocess.Popen(['python3 restapi.py'], shell=True)
time.sleep(5)

# system call, so that we wait until it is completed
os.system('python3 request.py')

# then terminate the sub processes
proc1.terminate()
proc2.terminate()

