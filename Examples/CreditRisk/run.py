#!/usr/bin/env python

import subprocess as p
                             # Legacy Examples
cases = [ "run_saccr.py",    # 68
	  "run_cpm.py"       # 43
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)

