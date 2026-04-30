#!/usr/bin/env python

import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

cases = [ "run_all.py", 
          "run_cbo.py"
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
