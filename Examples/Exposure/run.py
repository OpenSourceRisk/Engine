#!/usr/bin/env python

import subprocess as p
                              # Legacy Examples
cases = [ "run_swapflat.py",  # 1, 11
	  "run_swap.py",      # 2
          "run_fra.py",       # 23
          "run_swaption.py",  # 3, 4, 5
          "run_capfloor.py",  # 6
          "run_fx.py",        # 7
          "run_ccs.py",       # 8, 9
          "run_equity.py",    # 16
          "run_commodity.py", # 24
          "run_inflation.py", # 17, 32 
          "run_credit.py",    # 33
          "run_cmsspread.py", # 25
          "run_fbc.py",       # 64
          "run_longterm.py",  # 12
          "run_measures.py",  # 36
          "run_hw2f.py",      # 37 and 38
          "run_wwr.py",       # 34
          "run_flipview.py"   # 35
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)

