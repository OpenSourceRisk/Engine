#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_swapflat.py",         # 1, 11
    "run_swap.py",             # 2
    "run_swap_hw2f.py",        # same as 2, but with hw2f
    "run_fra.py",              # 23
    "run_swaption.py",         # 3, 4, 5
    "run_capfloor.py",         # 6
    "run_fx.py",               # 7
    "run_ccs.py",              # 8, 9
    "run_equity.py",           # 16
    "run_commodity.py",        # 24
    "run_inflation.py",        # 17, 32
    "run_credit.py",           # 33
    "run_cmsspread.py",        # 25
    "run_fbc.py",              # 64
    "run_longterm.py",         # 12
    "run_measures.py",         # 36
    "run_hw2f.py",             # 37 and 38
    "run_wwr.py",              # 34
    "run_flipview.py",         # 35
    "run_todayscashflows.py",  # 76
    "run_xva_corr.py",
    "run_hwhistoricalcalibration.py",
    "run_callable_bond.py",
    "run_fx_localvol.py"
]

sys.exit(run_scripts(cases))
