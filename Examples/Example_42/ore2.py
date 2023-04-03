'''
 Copyright (C) 2018-2023 Quaternion Risk Management Ltd

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org
 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>
 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
'''

import sys, time
from ORE import *

#############################################
# Read inputs from files and kick off ORE run

print ("Loading parameters...")
params = Parameters()
params.fromFile("Input/ore.xml")

print ("Creating OREApp...")
ore = OREApp(params, True)

print ("Running ORE process...");
ore.run()

print ("Running ORE process done");

###########################################
# Check the analytics we have requested/run

print ("\nRequested analytics:")
analyticTypes = ore.getAnalyticTypes()
for name in analyticTypes:
    print("analytc:", name)

print("\npress <enter> ...")
sys.stdin.readline()

#####################################
# List all results, reports and cubes

print ("Result reports:");
reportNames = ore.getReportNames()
for name in reportNames:
    print("report:", name)

print ("\nResult cubes:");
cubeNames = ore.getCubeNames()
for name in cubeNames:
    print("cube:", name)

print("\npress <enter> ...")
sys.stdin.readline()

#######################
# Access report details

# pick one
reportName = "exposure_nettingset_CPTY_A"
print ("Load report", reportName)
report = ore.getReport(reportName)

# see PlainInMemoryReport
columnTypes = { 0: "Size",
                1: "Real",
                2: "string",
                3: "Date",
                4: "Period" }

print ("columns:", report.columns())
print ("rows:", report.rows())
for i in range(0, report.columns()):
    print("colum", i, "header", report.header(i), "type", report.columnType(i), columnTypes[report.columnType(i)])

print("\npress <enter> ...")
sys.stdin.readline()

time = report.dataAsReal(2);
epe = report.dataAsReal(3);
ene = report.dataAsReal(4);
print ("#Time", "EPE")
for i in range(0, report.rows()):
    print("%5.2f  %12.2f  %12.2f" % (time[i], epe[i], ene[i]))

print("\npress <enter> ...")
sys.stdin.readline()

#####################
# Access cube details

cubeName = "cube"
print ("Load NPV cube:", cubeName)
cube = ore.getCube(cubeName)
print ("cube ids:", cube.numIds())
print ("cube samples:", cube.samples())
print ("cube dates:", cube.numDates())
print ("cube depth:", cube.depth())

print("\npress <enter> ...")
sys.stdin.readline()

cubeIds = cube.ids() 
cubeDates = cube.dates()

for i in range (0, cube.numIds()):
    for j in range (0, cube.numDates()):
        for k in range (0, cube.samples()):
            for d in range (0, cube.depth()):
                npv = cube.get(i, j, k, d)
                print ("%s %s %4d %d %10.2f" % (cubeIds[i], cubeDates[j].ISO(), k, d, npv))

###########################################################
# Access one analytic e.g. to query the market or portfolio

print("\npress <enter> ...")
sys.stdin.readline()

print("Inspect NPV analytic ...")
analytic = ore.getAnalytic("NPV")
market  = analytic.getMarket()

print("Market asof:", market.asofDate())

curve = market.discountCurve("EUR")
print("EUR discount factor at 10Y =", curve.discount(10 * Years))

portfolio = analytic.portfolio()
tradeIds = portfolio.ids();
print("Portfolio size:", portfolio.size())

for tid in tradeIds:
    trade = portfolio.get(tid)
    print("trade id=%s type=%s" % (trade.id(), trade.tradeType()))
    
print("Done")
