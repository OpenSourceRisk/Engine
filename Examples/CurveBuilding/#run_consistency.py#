import os
import sys
sys.path.append('Helpers/')
import TradeGenerator

sys.path.append('../')
sys.path.append('../')
from ore_examples_helper import OreExample

print("+-----------------------------------------------------+")
print("| Bootstrap Consistency                               |")
print("+-----------------------------------------------------+")

# Legacy example 26

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Running in currency trades")
oreex.run("Input/ore_consistency_ois.xml")

oreex.print_headline("Running EUR XOIS")
oreex.run("Input/ore_consistency_xoiseur.xml")

oreex.print_headline("Running USD XOIS")
oreex.run("Input/ore_consistency_xoisusd.xml")

