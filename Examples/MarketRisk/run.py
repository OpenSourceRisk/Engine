#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import run_scripts  # noqa

# Legacy Example numbers given below.
cases = [
    "run_sensi.py",           # 15, 40
    "run_sensismile.py",      # 22
    "run_parametricvar.py",   # 15
    "run_stress.py",          # 15, 77
    "run_parstress.py",       # 63
    "run_histsimvar.py",      # 58
    "run_histsimvartheta.py",
    "run_smrc.py",            # 68
    "run_pnlexplain.py",      # 62
    "run_parconversion.py",   # 50
    "run_basescenario.py",    # 57
    "run_zerotoparshift.py",  # 69
    "run_sensistress.py",
    "run_sensi_index_decomp.py",
    "run_stressscenariogeneration.py",
    "run_scenariostress.py",
    "run_correlation.py",
    "run_curvealgebra.py",
    "run_curves.py",
    "run_intradaypower.py"
]

sys.exit(run_scripts(cases))
