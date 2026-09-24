#!/usr/bin/env python

import subprocess
import sys
import time

print("+-----------------------------------------------------+")
print("| ORE-API                                             |")
print("+-----------------------------------------------------+")

proc1 = subprocess.Popen([sys.executable, "simplefileserver.py"])
time.sleep(5)

proc2 = subprocess.Popen([sys.executable, "restapi.py"])
time.sleep(5)

# wait until the request has completed
try:
    returncode = subprocess.call([sys.executable, "request.py"])
finally:
    # then terminate the sub processes
    proc1.terminate()
    proc2.terminate()

sys.exit(returncode)
