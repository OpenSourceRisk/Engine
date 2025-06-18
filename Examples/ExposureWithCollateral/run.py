#!/usr/bin/env python

import subprocess as p
                              # Legacy Examples
cases = [ "run_biweekly.py",  # 10
	  "run_closeout.py",  # 31
          "run_firstmpor.py"  # 72
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)
