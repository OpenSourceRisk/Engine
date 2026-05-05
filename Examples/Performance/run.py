#!/usr/bin/env python

import os
import sys
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv) > 1 else False)

# All individual ORE runs flattened for maximum parallelism
ore_runs = [
    # run_multithreading (Legacy Example 41)
    ("Multi-threading", "Input/ore_multi.xml"),
    # run_sensi (Legacy Example 61)
    ("Bump Sensitivities", "Input/ore_sensi.xml"),
    ("Sensi CG", "Input/ore_sensi_cg.xml"),
    ("Sensi AD", "Input/ore_sensi_ad.xml"),
    ("Sensi GPU", "Input/ore_sensi_gpu.xml"),
    # run_amclegacy (Legacy Example 56)
    ("AMC Legacy", "Input/ore_amc_legacy.xml"),
    # run_cvasensi (Legacy Example 56)
    ("CVA Sensi Bump", "Input/ore_cvasensi_bump.xml"),
    ("CVA Sensi AD", "Input/ore_cvasensi_ad.xml"),
    ("CVA Sensi GPU", "Input/ore_cvasensi_gpu.xml"),
]

# Get max parallel from environment variable, default to 1
max_parallel = int(os.getenv("EXAMPLES_PARALLEL", "1"))

def run_ore(label, xml):
    print(f"Running: {label} ({xml})")
    oreex.run(xml)
    print(f"Completed: {label} ({xml})")
    return (label, xml)

status = 0
with ThreadPoolExecutor(max_workers=max_parallel) as executor:
    futures = {executor.submit(run_ore, label, xml): (label, xml) for label, xml in ore_runs}
    for future in as_completed(futures):
        label, xml = futures[future]
        try:
            future.result()
        except Exception as e:
            print(f"Failed: {label} ({xml}) with error: {e}")
            status = 1

sys.exit(status)
