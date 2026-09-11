#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_biweekly.py",  # 10
    "run_closeout.py",  # 31
    "run_firstmpor.py"  # 72
]

sys.exit(run_scripts(cases))
