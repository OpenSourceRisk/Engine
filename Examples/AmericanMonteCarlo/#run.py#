#!/usr/bin/env python

import subprocess as p
                                       # Legacy Examples
cases = [ "run_benchmark.py",          # 39
	  "run_scriptedberm.py",       # 54
	  "run_scriptedtarf.py",       # 55
	  "run_forwardbond.py",        # 73
          "run_overlapping.py",        # 60
          "run_scenariostatistics.py"  # 75
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)
