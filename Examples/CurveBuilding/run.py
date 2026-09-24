#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below
cases = [
    "run_consistency.py",       # 26
    "run_discountratio.py",     # 28
    "run_fixedfloatccs.py",     # 29
    "run_prime.py",             # 30
    "run_bondyieldshifted.py",  # 49
    "run_centralbank.py",       # 53
    "run_sabr.py"               # 59
]

sys.exit(run_scripts(cases))
