import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+--------------------------------------------------+")
print("| Exposure: Include Effect of Today's Flows        |")
print("+--------------------------------------------------+")

# Legacy example 76

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for maturing portfolio with includeTodaysCashFlows=true")
oreex.run("Input/TodaysCashFlows/ore_maturing.xml")

oreex.print_headline("Run classic simulation for short term portfolio with includeTodaysCashFlows=true")
oreex.run("Input/TodaysCashFlows/ore_classic.xml")

oreex.print_headline("Run AMC simulation for short term portfolio with includeTodaysCashFlows=true")
oreex.run("Input/TodaysCashFlows/ore_amc.xml")
