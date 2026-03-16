import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+------------------------------------------------+")
print("| Exposure: Long Term Simulation                 |")
print("+------------------------------------------------+")

# Legacy example 12

oreex.run("Input/ore_longterm_1.xml")

oreex.run("Input/ore_longterm_2.xml")

oreex.setup_plot("exposure_longterm")
oreex.plot("longterm_1/exposure_trade_Swap_50y.csv", 2, 3, 'b', "Swap EPE (no horizon shift)")
oreex.plot("longterm_1/exposure_trade_Swap_50y.csv", 2, 4, 'r', "Swap ENE (no horizon shift)")
oreex.plot("longterm_2/exposure_trade_Swap_50y.csv", 2, 3, 'g', "Swap EPE (shifted horizon)")
oreex.plot("longterm_2/exposure_trade_Swap_50y.csv", 2, 4, 'y', "Swap ENE (shifted horizon)")

oreex.decorate_plot(title="Simulated exposures with and without horizon shift")
oreex.save_plot_to_file()

oreex.setup_plot("exposure_longterm_rates")
oreex.plot_zeroratedist("longterm_1/scenariodump.csv", 0, 23, 5, 'r', label="No horizon shift", title="")
oreex.plot_zeroratedist("longterm_2/scenariodump.csv", 0, 23, 5, 'b', label="Shifted horizon", title="5y zero rate (EUR) distribution with and without horizon shift")
oreex.save_plot_to_file()

