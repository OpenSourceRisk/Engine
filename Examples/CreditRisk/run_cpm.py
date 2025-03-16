#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

print("+-----------------------------------------------------+")
print("| Credit Portfolio Model                              |")
print("+-----------------------------------------------------+")

oreex.print_headline("Run ORE for a single Bond")
orexml="Input/ore.xml"
oreex.run(orexml)
case="bond1"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for Bond and Swap")
orexml="Input/ore1.xml"
oreex.run(orexml)
case="bond_swap"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for 3 Bonds")
orexml="Input/ore2.xml"
oreex.run(orexml)
case="bond3"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for 10 bonds")
orexml="Input/ore3.xml"
oreex.run(orexml)
case="bond10"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for 10 bonds (Evaluation = TerminalSimulation)")
orexml="Input/ore3_ts.xml"
oreex.run(orexml)
case="bond10_ts"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

oreex.print_headline("Run ORE for Bond and CDS")
orexml="Input/ore4.xml"
oreex.run(orexml)
case="bond_cds"
targetFileName = "CreditPortfolioModel/credit_migration_" + case + "_11.csv"
oreex.setup_plot("cdf_" + case)
oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()
oreex.setup_plot("pdf_" + case)
oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
oreex.save_plot_to_file()

#oreex.print_headline("Run ORE for 100 Bonds")
#orexml="Input/ore100.xml"
#oreex.run(orexml)
#case="bond100"
#oreex.setup_plot("cdf_" + case)
#oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
#oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
#oreex.save_plot_to_file()
#oreex.setup_plot("pdf_" + case)
#oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
#oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
#oreex.save_plot_to_file()
