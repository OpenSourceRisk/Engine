
# Copyright (C) 2018 Quaternion Risk Manaement Ltd
# All rights reserved.

from ORE import *

def formatPrice(p,digits=2):
    format = '%%.%df' % digits
    return format % p

# global data
calendar = TARGET()
todaysDate = Date(4, October, 2018);
Settings.instance().evaluationDate = todaysDate

# instrument
name = "Natural Gas";
currency = GBPCurrency();
strikePrice = 100.0;
quantity = 1.0;
position = Position.Long;

maturityDate = Date(4, October, 2022);

# market
dates = [ Date(20,12,2018),
          Date(20, 3,2019),
          Date(19, 6,2019),
          Date(18, 9,2019),
          Date(18,12,2019),
          Date(19, 3,2020),
          Date(18, 6,2020),
          Date(17, 9,2020),
          Date(17,12,2020) ]
    
quotes = [ QuoteHandle(SimpleQuote(100.0)),
           QuoteHandle(SimpleQuote(100.25)),
           QuoteHandle(SimpleQuote(100.75)),
           QuoteHandle(SimpleQuote(101.0)),
           QuoteHandle(SimpleQuote(101.25)),
           QuoteHandle(SimpleQuote(101.50)),
           QuoteHandle(SimpleQuote(101.75)),
           QuoteHandle(SimpleQuote(102.0)),
           QuoteHandle(SimpleQuote(102.25)) ]

tsDayCounter = Actual365Fixed()
currency = EURCurrency();

# price curve
priceCurve = LinearInterpolatedPriceCurve(todaysDate, dates, quotes, tsDayCounter, currency);
priceCurve.enableExtrapolation();
priceTermStructure = RelinkablePriceTermStructureHandle();
priceTermStructure.linkTo(priceCurve)

# discount curve
flatForward = FlatForward(todaysDate, 0.03, tsDayCounter);
discountTermStructure = RelinkableYieldTermStructureHandle()
discountTermStructure.linkTo(flatForward)

engine = DiscountingCommodityForwardEngine(discountTermStructure)

index = CommoditySpotIndex(name, calendar, priceTermStructure)
instrument = CommodityForward(index, currency, position, quantity, maturityDate, strikePrice);

instrument.setPricingEngine(engine)

print("Commodity Forward, Name='" + name + "', NPV=" + formatPrice(instrument.NPV()) + " " + instrument.currency().code())
