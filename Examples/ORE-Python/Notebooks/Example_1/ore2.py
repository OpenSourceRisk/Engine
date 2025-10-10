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

import sys, time, math
from ORE import *

#############################################
# Read inputs from files and kick off ORE run

print ("Loading parameters...")
params = Parameters()
params.fromFile("Input/ore.xml")

print ("Creating OREApp...")
ore = OREApp(params)

print ("Running ORE process...");
ore.run()

print ("Running ORE process done");

###########################################
# Check the analytics we have requested/run

print ("\nRequested analytics:")
analyticTypes = ore.getAnalyticTypes()
for name in analyticTypes:
    print("-", name)

print("\npress <enter> ...")
sys.stdin.readline()

##################
# List all reports

print ("Result reports:");
reportNames = ore.getReportNames()
for name in reportNames:
    print("-", name)

print("\npress <enter> ...")
sys.stdin.readline()

#############################
# Process the cashflow report

reportName = "cashflow"
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

flowNo = report.dataAsSize(2)
legNo = report.dataAsSize(3)
date = report.dataAsDate(4)
flowType = report.dataAsString(5) 
amount = report.dataAsReal(6)
print ("#FlowNo", "LegNo", "Date", "FlowType", "Amount")
for i in range(0, report.rows()):
    print("%d %d %s %s %.2f" % (flowNo[i], legNo[i], date[i].ISO(), flowType[i], amount[i]))

print("\npress <enter> ...")
sys.stdin.readline()

###########################
# Process the curves report

reportName = "curves"
print ("Load report", reportName)
report = ore.getReport(reportName)
print ("columns:", report.columns())
print ("rows:", report.rows())
for i in range(0, report.columns()):
    print("colum", i, "header", report.header(i), "type", report.columnType(i), columnTypes[report.columnType(i)])

print("\npress <enter> ...")
sys.stdin.readline()

dc = Actual365Fixed()
asof = ore.getInputs().asof()
date = report.dataAsDate(1)
discount = report.dataAsReal(2)
print ("#Index", "Date", "Discount")
for i in range(0, report.rows()):
    time = dc.yearFraction(asof, date[i])
    zero = -math.log(discount[i]) / time
    print("%d %s %.4f %.8f %.8f" % (i, date[i].ISO(), time, discount[i], zero))

print("Done")
