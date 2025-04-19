
# Copyright (C) 2018 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *
import xml.etree.ElementTree as ET
import os

orexml = "Input/ore_market.xml"
print ("Load parameters from", orexml)
params = Parameters()
params.fromFile(orexml)

print ("Build ORE App...")
ore = OREApp(params)
print ("Run ORE to build inputs...") 
ore.run()
inputs = ore.getInputs()

print ("Get the portfolio from xml...")
portfolioFile = os.path.join("Input","portfolio.xml")
portfolioXml = ET.parse(portfolioFile).getroot()
portfolioXmlStr = ET.tostring(portfolioXml,encoding="unicode")

print ("Add the portfolio and request NPV...")
inputs.setPortfolio(portfolioXmlStr)
inputs.insertAnalytic("NPV")

print ("Run ORE process...")
ore.run()

print ("Get NPV analytic...")
analytic = ore.getAnalytic("NPV")
print("Get the built portfolio from the NPV analytic")
portfolio = analytic.portfolio();

portfolioSize = portfolio.size()
#print("...portfolio.size() is of type", type(portfolioSize))
print("... portfolio size = ", str(portfolioSize))
ids = portfolio.ids();
for id in ids:
    print(id)
    trn = portfolio.get(id)
    #print(" trn is of type ", type(trn))
    print(" trade id = ", trn.id())
    print(" trade type is ", trn.tradeType())
    print(" trade notional is ", trn.notional())
    pgInst = trn.instrument()
    #print(" inst type is ", type(pgInst))
    trnPv = pgInst.NPV()
    print(" instWrap pv = ", str(trnPv))
    qlInst = pgInst.qlInstrument()
    #print(" qlInst is of type ", type(qlInst))
    qlPv = qlInst.NPV()
    print(" qlInst pv = ", str(qlPv))
    isExp = qlInst.isExpired()
    print(" is expired? = ", isExp)
    legPg = trn.legs()
    #print(" legsVec is of type ", type(legPg))
    for leg in legPg:
        #print("     leg is of type ", type(leg))
        for flow in leg:
            print("          flow as of ", flow.date().ISO(), " is ", flow.amount())

print("Done.")
