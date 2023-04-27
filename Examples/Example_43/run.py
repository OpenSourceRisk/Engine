#!/usr/bin/env python

import sys
sys.path.append('../')
from ore_examples_helper import OreExample

oreex = OreExample(sys.argv[1] if len(sys.argv)>1 else False)

oreex.print_headline("Run ORE for a single Bond")
orexml="Input/ore.xml"
case="bond1"
sourceFileName = "credit_migration_11.csv"
targetFileName = "credit_migration_" + case + ".csv"
oreex.run(orexml)
oreex.copy_file(sourceFileName, targetFileName)
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
case="bond_swap"
sourceFileName = "credit_migration_11.csv"
targetFileName = "credit_migration_" + case + ".csv"
oreex.run(orexml)
oreex.copy_file(sourceFileName, targetFileName)
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
case="bond3"
sourceFileName = "credit_migration_11.csv"
targetFileName = "credit_migration_" + case + ".csv"
oreex.run(orexml)
oreex.copy_file(sourceFileName, targetFileName)
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
case="bond10"
sourceFileName = "credit_migration_11.csv"
targetFileName = "credit_migration_" + case + ".csv"
oreex.run(orexml)
oreex.copy_file(sourceFileName, targetFileName)
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
case="bond_cds"
sourceFileName = "credit_migration_11.csv"
targetFileName = "credit_migration_" + case + ".csv"
oreex.run(orexml)
oreex.copy_file(sourceFileName, targetFileName)
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
#case="bond100"
#sourceFileName = "credit_migration_11.csv"
#targetFileName = "credit_migration_" + case + ".csv"
#oreex.run(orexml)
#oreex.copy_file(sourceFileName, targetFileName)
#oreex.setup_plot("cdf_" + case)
#oreex.plot(targetFileName, 0, 2, 'b', label="Loss")
#oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="CDF", xlabel="Loss", y_format_as_int=False)
#oreex.save_plot_to_file()
#oreex.setup_plot("pdf_" + case)
#oreex.plot(targetFileName, 0, 1, 'b', label="Loss")
#oreex.decorate_plot(title="Example 43 - Loss distribution", ylabel="PDF", xlabel="Loss", y_format_as_int=False)
#oreex.save_plot_to_file()
