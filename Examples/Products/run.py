#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

cases = [
    "run_all.py",
    "run_cbo.py"
]

sys.exit(run_scripts(cases))
