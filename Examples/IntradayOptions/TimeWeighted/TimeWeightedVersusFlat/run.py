#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent.parent.parent))
from ore_examples_helper import run_scripts  # noqa

# These cases require an ORE build with QL_HIGH_RESOLUTION_DATE=ON, so this folder is not part of
# get_list_of_new_examples() in ore_examples_helper.py.
cases = [
    "run_time_weighted_vs_flat.py",
]

sys.exit(run_scripts(cases))
