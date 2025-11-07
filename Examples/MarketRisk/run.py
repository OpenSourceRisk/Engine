#!/usr/bin/env python

import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

# List of scripts to run
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
          "run_sensistress.py",
          "run_sensi_index_decomp.py",
          "run_stressscenariogeneration.py",
          "run_scenariostress.py",
          "run_correlation.py",
          "run_curvealgebra.py"
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

