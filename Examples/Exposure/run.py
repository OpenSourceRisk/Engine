#!/usr/bin/env python

import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
                              # Legacy Examples
cases = [ "run_swapflat.py",  # 1, 11
          "run_swap.py",      # 2
          "run_swap_hw2f.py", # same as 2, but with hw2f
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
          "run_flipview.py",   # 35
          "run_xva_corr.py",
          "run_hwhistoricalcalibration.py"
         ]

# Get max parallel from environment variable, default to 1
max_parallel = int(os.getenv("EXAMPLES_PARALLEL", "1"))

def run_script(script_name):
    cmd = ["python3", script_name]
    print("Calling:", " ".join(cmd))
    return subprocess.call(cmd)

# Run scripts in parallel
with ThreadPoolExecutor(max_workers=max_parallel) as executor:
    futures = [executor.submit(run_script, case) for case in cases]
    for future in as_completed(futures):
        result = future.result()
        print(f"Script finished with exit code: {result}")
