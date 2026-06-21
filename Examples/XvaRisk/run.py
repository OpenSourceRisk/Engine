#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_stress.py",          # 67
    "run_xvaexplain.py",      # 70
    "run_sensi.py",           # 68
    "run_sacva.py",           # 68
    "run_bacva.py"            # 68
]

sys.exit(run_scripts(cases))
