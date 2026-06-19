#!/usr/bin/env python

import os
from concurrent.futures import ThreadPoolExecutor, as_completed
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import OreExample  # noqa
from ore_examples_helper import print_on_console  # noqa

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
    print_on_console(f"Running: {label} ({xml})")
    oreex.run(xml)
    print_on_console(f"Completed: {label} ({xml})")

failed = False
with ThreadPoolExecutor(max_workers=max_parallel) as executor:
    futures = {executor.submit(run_ore, label, xml): (label, xml) for label, xml in ore_runs}
    for future in as_completed(futures):
        label, xml = futures[future]
        try:
            result = future.result()
            print_on_console(f"{label} ({xml}) completed successfully")
        except Exception as e:
            print_on_console(f"{label} ({xml}) failed with error: {e}")
            failed = True

sys.exit(1 if failed else 0)
