#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_simm.py",                   # 44
    "run_dim.py",                    # 13
    "run_dim2.py",
    "run_dim2_regregressiontest.py"  # test dyn simm against expected output
]

sys.exit(run_scripts(cases))
