
# Copyright (C) 2018 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *

today = Settings.instance().evaluationDate 
print ("\ntoday's date is %s" % today.ISO())

# Global data
calendar = TARGET()
valuationDate = Date(4, October, 2018);
Settings.instance().evaluationDate = valuationDate
print ("valuation date is %s" % valuationDate.ISO())

# Commodity forward instrument
name = "Natural Gas";
currency = GBPCurrency();
strikePrice = 100.0;
quantity = 200.0;
position = Position.Long;
maturityDate = Date(4, October, 2022);

# Market
dates = [ Date(20,12,2018),
          Date(20,12,2022) ]
    
quotes = [ QuoteHandle(SimpleQuote(102.0)),
           QuoteHandle(SimpleQuote(102.0)) ]
           
dayCounter = Actual365Fixed()

# Price curve
priceCurve = LinearInterpolatedPriceCurve(valuationDate, dates, quotes, dayCounter, currency);
priceCurve.enableExtrapolation();
priceTermStructure = RelinkablePriceTermStructureHandle();
priceTermStructure.linkTo(priceCurve)

# Discount curve
flatForward = FlatForward(valuationDate, 0.03, dayCounter);
discountTermStructure = RelinkableYieldTermStructureHandle()
discountTermStructure.linkTo(flatForward)

engine = DiscountingCommodityForwardEngine(discountTermStructure)

index = CommoditySpotIndex(name, calendar, priceTermStructure)
instrument = CommodityForward(index, currency, position, quantity, maturityDate, strikePrice);

instrument.setPricingEngine(engine)

print("\nCommodity Forward on '%s' NPV=%.2f %s\n" % (name, instrument.NPV(), instrument.currency().code()))
