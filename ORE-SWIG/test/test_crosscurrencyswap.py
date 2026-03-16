"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.

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

"""


import ORE as qle
import unittest


class CrossCurrencySwapTest(unittest.TestCase):
    def setUp(self):
        print('Set Up of the Instrument Arguments')

        # Today:
        self.todayDate = qle.Date(11, 9, 2018)

        # Notional:
        self.nominal1 = 10000000
        self.nominal2 = 12000000

        # Currencies:
        self.currency1 = qle.USDCurrency()
        self.currency2 = qle.EURCurrency()

        # Leg Details
        self.settlementDate = qle.Date(13, 9, 2018)
        self.length = 5
        self.calendar = qle.TARGET()
        self.tsDayCounter = qle.Actual365Fixed()
        self.busDayConvention = qle.Following
        self.forwardStart = self.calendar.advance(self.settlementDate, 1, qle.Years)
        self.forwardEnd = self.calendar.advance(self.forwardStart, self.length, qle.Years)

        # Floating Leg Schedule
        self.floatingLegFrequency = qle.Semiannual
        self.floatingLegTenor = qle.Period(6, qle.Months)
        self.floatingLegAdjustment = qle.ModifiedFollowing
        self.floatingSchedule = qle.Schedule(self.forwardStart,
                                             self.forwardEnd,
                                             self.floatingLegTenor,
                                             self.calendar,
                                             self.floatingLegAdjustment,
                                             self.floatingLegAdjustment,
                                             qle.DateGeneration.Forward,
                                             False)

        # Floating Leg Index (from discounted term structure)
        self.flatForwardUSD = qle.FlatForward(self.todayDate, 0.005, self.tsDayCounter)
        self.discountTermStructureUSD = qle.RelinkableYieldTermStructureHandle()
        self.discountTermStructureUSD.linkTo(self.flatForwardUSD)
        self.indexUSD = qle.USDLibor(qle.Period(6, qle.Months), self.discountTermStructureUSD)

        # Floating Leg
        self.floatingLegUSD = qle.IborLeg([self.nominal1],
                                          self.floatingSchedule,
                                          self.indexUSD,
                                          self.indexUSD.dayCounter())

        # Fixed Schedule
        self.fixedLegTenor = qle.Period(1, qle.Years)
        self.fixedLegAdjustment = qle.Unadjusted
        self.fixedLegDayCounter = qle.Thirty360(qle.Thirty360.USA)
        self.fixedSchedule = qle.Schedule(self.forwardStart,
                                          self.forwardEnd,
                                          self.fixedLegTenor,
                                          self.calendar,
                                          self.fixedLegAdjustment,
                                          self.fixedLegAdjustment,
                                          qle.DateGeneration.Forward,
                                          False)

        # Fixed Leg Index (from discounted term structure)
        self.flatForwardEUR = qle.FlatForward(self.todayDate, 0.03, self.tsDayCounter);
        self.discountTermStructureEUR = qle.RelinkableYieldTermStructureHandle()
        self.discountTermStructureEUR.linkTo(self.flatForwardEUR)
        self.indexEUR = qle.EURLibor(qle.Period(6, qle.Months), self.discountTermStructureEUR)
        self.couponRate = 0.03

        # Fixed Leg
        self.fixedLegEUR = qle.FixedRateLeg(self.fixedSchedule,
                                            self.indexEUR.dayCounter(),
                                            [self.nominal2],
                                            [self.couponRate])

        # FX Quote
        self.fxQuote = qle.QuoteHandle(qle.SimpleQuote(1/0.8))

    def testInstrumentCreation(self):
        print('Creation the Instrument Object')

        self.instrument = qle.CrossCcySwap(self.floatingLegUSD,
                                           self.currency1,
                                           self.fixedLegEUR,
                                           self.currency2)

        print("Creation of  Instrument Engine")
        self.engine = qle.CrossCcySwapEngine(self.currency1,
                                             self.discountTermStructureEUR,
                                             self.currency2,
                                             self.discountTermStructureUSD,
                                             self.fxQuote,
                                             False,
                                             self.settlementDate,
                                             self.todayDate)

        print("Pricing of the Instrument with the Engine")
        self.instrument.setPricingEngine(self.engine)


if __name__ == '__main__':
    print('testing ORE ' + qle.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(CrossCurrencySwapTest, 'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
