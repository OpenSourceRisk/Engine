
# Copyright (C) 2018 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *

orexml = "../Input/ore_market.xml"
print ("Run ORE using", orexml)
params = Parameters()
params.fromFile(orexml)
ore = OREApp(params)
ore.run()

# Get first analytic, there should be just one (MARKETDATA)
for a in ore.getAnalyticTypes():
    print ("Get analytic", a)
    analytic = ore.getAnalytic(a)
    print("Get market object from analytic", a)    
    market  = analytic.getMarket()
    break

asof = market.asofDate();
print ("Market asof date", asof)

ccy = "EUR"
index = "EUR-EURIBOR-6M"
print ("Get term structures for ccy ", ccy, "and index", index);

discountCurve = market.discountCurve(ccy)
print ("   discount curve is of type", type(discountCurve))

iborIndex = market.iborIndex(index)
print ("   ibor index is of type", type(iborIndex))

forwardCurve = iborIndex.forwardingTermStructure()
print ("   forward curve is of type", type(forwardCurve))

print ("Evaluate term structures");
date = asof + 10*Years;
zeroRateDc = Actual365Fixed()
discount = discountCurve.discount(date)
zero = discountCurve.zeroRate(date, zeroRateDc, Continuous)
fwdDiscount = forwardCurve.discount(date)
fwdZero = forwardCurve.zeroRate(date, zeroRateDc, Continuous)
print ("   10y discount factor (discount curve) is", discount)
print ("   10y discout factor (forward curve) is", fwdDiscount)
print ("   10y zero rate (discount curve) is", zero)
print ("   10y zero rate (forward curve) is", fwdZero)

print("Done")
