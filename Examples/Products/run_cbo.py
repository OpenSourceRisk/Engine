#!/usr/bin/env python

import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
from ore_examples_helper import OreExample  # noqa

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Products: CBO                                       |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE to price the CBO example")
oreex.run("Input_CBO/ore.xml")
