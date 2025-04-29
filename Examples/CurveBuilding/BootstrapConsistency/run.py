import os
import sys
sys.path.append('Helpers/')
import TradeGenerator


sys.path.append('../')
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Running in currency trades")
oreex.run("Input/ois_ore.xml")

oreex.print_headline("Running EUR out of currency trades")
oreex.run("Input/EUR_xois_ore.xml")

oreex.print_headline("Running USD out of currency trades")
oreex.run("Input/USD_xois_ore.xml")

