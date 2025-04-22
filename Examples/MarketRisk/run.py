#!/usr/bin/env python

import subprocess as p
                                    # Legacy Examples
cases = [ "run_sensi.py",           # 15, 40
          "run_sensismile.py",      # 22
          "run_parametricvar.py",   # 15
          "run_stress.py",          # 15, 77
          "run_parstress.py",       # 63
          "run_histsimvar.py",      # 58
          "run_smrc.py",            # 68
          "run_pnlexplain.py",      # 62
          "run_parconversion.py",   # 50
          "run_basescenario.py",    # 57
          "run_zerotoparshift.py",  # 69
          "run_sensistress.py"
         ]

for c in cases:
    cmd = "python3 " + c;
    print ("Calling:", cmd)
    p.call(cmd, shell=True)
