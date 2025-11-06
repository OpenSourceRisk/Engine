#!/usr/bin/env python

import subprocess as p
                                     # Legacy Examples
cases = [ "run_consistency.py",      # 26
          "run_discountratio.py",    # 28
          "run_fixedfloatccs.py",    # 29
          "run_prime.py",            # 30
          "run_bondyieldshifted.py", # 49
          "run_centralbank.py",      # 53
          "run_sabr.py"              # 59
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)
