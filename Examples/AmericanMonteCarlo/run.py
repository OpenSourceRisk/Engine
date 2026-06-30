#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_benchmark.py",           # 39
    "run_scriptedberm.py",        # 54
    "run_fxtarf.py",              # 55
    "run_forwardbond.py",         # 73
    "run_overlapping.py",         # 60
    "run_scenariostatistics.py",  # 75
    "run_fx_options.py",
    "run_fx_localvol.py",
    "run_scripted_amc.py",        # scripted trade types AMC
    "run_fx_baskets.py"           # FX basket trades AMC (requires market data)
]

sys.exit(run_scripts(cases))
