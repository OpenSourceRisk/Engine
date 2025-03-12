#!/usr/bin/env python

import subprocess as p
                              # Legacy Examples
cases = [ "run_simm.py",      # 44
	  "run_dim.py"        # 13
]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)
