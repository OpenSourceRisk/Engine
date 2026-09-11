"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../build/ore/ORE-SWIG'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '../../build/ore/ORE-SWIG/RelWithDebInfo'))

from ORE import *
import unittest

class EquityMarginCouponTest(unittest.TestCase):
    def setUp(self):
        """Set up EquityMarginCoupon test following EquityCouponTest pattern."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate
        
        # Flat yield curve
        self.dayCounter = Actual365Fixed()
        self.flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        self.ytsHandle = RelinkableYieldTermStructureHandle(self.flatForward)
        
        # Equity index setup
        self.eqName = "EQ-MARGIN-TEST"
        self.eqCurrency = USDCurrency()
        self.eqCalendar = UnitedStates(UnitedStates.NYSE)
        self.eqSpot = QuoteHandle(SimpleQuote(100.0))
        
        # EquityIndex2 constructor (name, calendar, currency, spot, rate, dividend)
        self.equityIndex = EquityIndex2(
            self.eqName, self.eqCalendar, self.eqCurrency,
            self.eqSpot, self.ytsHandle, self.ytsHandle)
        
        # Coupon parameters
        self.startDate = Date(15, January, 2026)
        self.endDate = Date(15, April, 2026)
        self.paymentDate = Date(17, April, 2026)
        self.nominal = 1000000.0
        self.rate = 0.05
        self.marginFactor = 0.02
        self.fixingDays = 0

    def testEquityMarginCouponConstruction(self):
        """Test EquityMarginCoupon construction."""
        coupon = EquityMarginCoupon(
            self.paymentDate,
            self.nominal,
            self.rate,
            self.marginFactor,
            self.startDate,
            self.endDate,
            self.fixingDays,
            self.equityIndex,
            self.dayCounter)
        
        self.assertIsNotNone(coupon)
        self.assertEqual(coupon.date(), self.paymentDate)
        self.assertAlmostEqual(coupon.nominal(), self.nominal, delta=1e-10)

    def testEquityMarginCouponAccessors(self):
        """Test EquityMarginCoupon accessors for all AC requirements."""
        coupon = EquityMarginCoupon(
            self.paymentDate,
            self.nominal,
            self.rate,
            self.marginFactor,
            self.startDate,
            self.endDate,
            self.fixingDays,
            self.equityIndex,
            self.dayCounter,
            False,  # isTotalReturn
            1.0,  # dividendFactor
            False,  # notionalReset
            100.0,  # initialPrice
            1.0,  # quantity
            self.startDate,  # fixingStartDate
            self.endDate,  # fixingEndDate
            self.startDate,  # refPeriodStart
            self.endDate,  # refPeriodEnd
            Date(),  # exCouponDate
            1.0)  # multiplier
        
        # Test all AC accessors
        self.assertAlmostEqual(coupon.marginFactor(), self.marginFactor, delta=1e-10)
        self.assertAlmostEqual(coupon.multiplier(), 1.0, delta=1e-10)
        self.assertAlmostEqual(coupon.quantity(), 1.0, delta=1e-10)
        self.assertFalse(coupon.isTotalReturn())
        self.assertIsNotNone(coupon.equityCurve())
        self.assertAlmostEqual(coupon.fxRate(), 1.0, delta=1e-10)
        self.assertEqual(len(coupon.fixingDates()), 2)

    def testEquityMarginCouponPricerSetup(self):
        """Test EquityMarginCouponPricer can be set and retrieved."""
        coupon = EquityMarginCoupon(
            self.paymentDate,
            self.nominal,
            self.rate,
            self.marginFactor,
            self.startDate,
            self.endDate,
            self.fixingDays,
            self.equityIndex,
            self.dayCounter)
        
        pricer = EquityMarginCouponPricer()
        coupon.setPricer(pricer)
        retrievedPricer = coupon.pricer()
        self.assertIsNotNone(retrievedPricer)

    def testEquityMarginCouponPricerInitialize(self):
        """Test EquityMarginCouponPricer.initialize() and rate() methods."""
        coupon = EquityMarginCoupon(
            self.paymentDate,
            self.nominal,
            self.rate,
            self.marginFactor,
            self.startDate,
            self.endDate,
            self.fixingDays,
            self.equityIndex,
            self.dayCounter)
        
        pricer = EquityMarginCouponPricer()
        pricer.initialize(coupon)
        rateVal = pricer.rate()
        self.assertIsInstance(rateVal, float)

if __name__ == '__main__':
    unittest.main()


