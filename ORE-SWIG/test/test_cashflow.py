"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import ORE
import unittest
import logging

class FXLinkedCashFlowTest(unittest.TestCase):
    def setUp(self):
        """ Set-up FX Linked Cash Flow """
        self.cashFlowDate=Date(5,January,2015)
        self.fixingDate=Date(5,January,2017)
        self.foreignAmount=1000000.0
        self.familyName="FX::USDJPY"
        self.fixingDays=0
        self.sourceCurrency=USDCurrency()
        self.targetCurrency=JPYCurrency()
        self.fixingCalendar=UnitedStates(UnitedStates.NYSE)
        self.todayDate=Date(5, January, 2016)
        self.tsDayCounter=Actual360()
        self.flatForwardUSD=FlatForward(self.todayDate, 0.005, self.tsDayCounter)
        self.sourceYts=RelinkableYieldTermStructureHandle(self.flatForwardUSD)
        self.flatForwardJPY=FlatForward(self.todayDate, 0.03, self.tsDayCounter)
        self.targetYts=RelinkableYieldTermStructureHandle(self.flatForwardJPY)
        self.quote = SimpleQuote(123.45)
        self.fxspot = RelinkableQuoteHandle(self.quote)
        self.fxindex=FxIndex(self.familyName,self.fixingDays,self.sourceCurrency,self.targetCurrency,self.fixingCalendar,self.fxspot, self.sourceYts,self.targetYts)
        self.fxlinkedcashflow=FXLinkedCashFlow(self.cashFlowDate,self.fixingDate,self.foreignAmount,self.fxindex)
        self.fxlinkedcashflow1=FXLinkedCashFlow(self.cashFlowDate,self.cashFlowDate,self.foreignAmount,self.fxindex)
        self.fxlinkedcashflow2=FXLinkedCashFlow(self.todayDate, self.todayDate, self.foreignAmount, self.fxindex)
        self.fxindex.addFixing(self.cashFlowDate, 112.0)
        self.fxindex.addFixing(self.todayDate, self.quote.value())


    def testSimpleInspectors(self):
        """ Test FX Linked Cash simple inspectors. """
        self.assertEqual(self.fxlinkedcashflow.date(),self.cashFlowDate)
        self.assertEqual(self.fxlinkedcashflow.fxFixingDate(),self.fixingDate)
        self.assertEqual(self.fxlinkedcashflow1.date(),self.cashFlowDate)
        self.assertEqual(self.fxlinkedcashflow1.fxFixingDate(),self.cashFlowDate)
        
            
    def testConsistency(self):
        """ Test consistency of FX Linked Cash Flow fair price and NPV() """
        self.assertAlmostEqual(self.fxlinkedcashflow1.amount(), 112000000.0, None, "Historical Flow is Incorrect", 1e-10)
        self.assertAlmostEqual(self.fxlinkedcashflow2.amount(), 123450000.0, None, "Todays Flow is Incorrect", 1e-10)
        #self.assertAlmostEqual(self.fxlinkedcashflow.amount(), 0)

class FloatingRateFXLinkedNotionalCouponTest(unittest.TestCase):
    def setUp(self):
        """ Set-up Floating Rate FX Linked Notional Coupon """
        self.foreignAmount=1000.0
        self.fxFixingDate=Date(1,October,2018)
        self.familyName="ECB"
        self.fixingDays=2
        self.sourceCurrency=USDCurrency()
        self.targetCurrency=EURCurrency()
        self.fixingCalendar=UnitedStates(UnitedStates.NYSE)
        self.todayDate=Date(11, November, 2018)
        self.tsDayCounter=Actual360()
        self.flatForwardUSD=FlatForward(self.todayDate, 0.005, self.tsDayCounter)
        self.sourceYts=RelinkableYieldTermStructureHandle(self.flatForwardUSD)
        self.flatForwardEUR=FlatForward(self.todayDate, 0.03, self.tsDayCounter)
        self.targetYts=RelinkableYieldTermStructureHandle(self.flatForwardEUR)
        self.fxindex=FxIndex(self.familyName,self.fixingDays,self.sourceCurrency,self.targetCurrency,self.fixingCalendar,self.sourceYts,self.targetYts)
        self.paymentDate=Date(1,November,2018)
        self.startDate=Date(1,October,2018)
        self.endDate=Date(1,November,2018)
        self.fixingDays=2
        self.gearing=1.0
        self.spread=0.0
        self.refPeriodStart=Date(1,October,2018)
        self.refPeriodEnd=Date(1,November,2018)
        self.dayCounter=Actual360()
        self.isInArrears=False
        self.tenor=Period(3,Months)
        self.settlementDays=2
        self.currency=GBPCurrency()
        self.floatIndex=USDLibor(self.tenor,self.sourceYts)
        self.undCpn = IborCoupon(self.paymentDate,self.foreignAmount, self.startDate,self.endDate,self.fixingDays,self.floatIndex,self.gearing,self.spread,self.refPeriodStart,self.refPeriodEnd,self.dayCounter)
        self.floatingratefxlinkednotionalcoupon=FloatingRateFXLinkedNotionalCoupon(self.fxFixingDate,self.foreignAmount,self.fxindex,self.undCpn)
        
    def testSimpleInspectors(self):
        """ Test Floating Rate FX Linked Notional Coupon inspectors. """
        self.assertEqual(self.paymentDate,self.floatingratefxlinkednotionalcoupon.date())
        
        
            
    def testConsistency(self):
        """ Test consistency of FX Linked Cash Flow fair price and NPV() """

        
class EquityCouponTest(unittest.TestCase):
    def setUp(self):
        """Set up an EquityCoupon with a simple EquityIndex2 and flat curve."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate

        self.dayCounter = Actual365Fixed()
        self.flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        self.ytsHandle = RelinkableYieldTermStructureHandle(self.flatForward)

        self.eqName = "EQ-TEST"
        self.eqCurrency = USDCurrency()
        self.eqCalendar = UnitedStates(UnitedStates.NYSE)
        self.eqSpot = QuoteHandle(SimpleQuote(100.0))

        self.equityIndex = EquityIndex2(
            self.eqName, self.eqCalendar, self.eqCurrency,
            self.eqSpot, self.ytsHandle, self.ytsHandle)

        self.startDate = Date(15, January, 2026)
        self.endDate = Date(15, April, 2026)
        self.paymentDate = Date(17, April, 2026)
        self.nominal = 1000000.0
        self.fixingDays = 0

    def testEquityCouponConstruction(self):
        """Test EquityCoupon construction and basic accessors."""
        coupon = EquityCoupon(
            self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.fixingDays, self.equityIndex,
            self.dayCounter, EquityReturnType_Total,
            1.0, False)

        self.assertEqual(coupon.date(), self.paymentDate)
        self.assertAlmostEqual(coupon.nominal(), self.nominal, delta=1e-10)
        self.assertEqual(coupon.returnType(), EquityReturnType_Total)
        self.assertAlmostEqual(coupon.dividendFactor(), 1.0, delta=1e-10)
        self.assertFalse(coupon.notionalReset())
        self.assertEqual(coupon.fixingStartDate(), self.startDate)
        self.assertEqual(coupon.fixingEndDate(), self.endDate)

    def testEquityCouponPricer(self):
        """Test EquityCouponPricer attach and initialize."""
        coupon = EquityCoupon(
            self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.fixingDays, self.equityIndex,
            self.dayCounter, EquityReturnType_Total,
            1.0, False)

        pricer = EquityCouponPricer()
        coupon.setPricer(pricer)
        self.assertIsNotNone(coupon.pricer())

        pricer.initialize(coupon)
        rate = pricer.swapletRate()
        self.assertIsInstance(rate, float)

    def testEquityLegConstruction(self):
        """Test EquityLeg builder produces a non-empty Leg."""
        schedule = Schedule(
            self.startDate, Date(15, January, 2027),
            Period(3, Months), self.eqCalendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)

        leg = EquityLeg(
            schedule=schedule,
            equityCurve=self.equityIndex,
            notionals=[self.nominal],
            paymentDayCounter=self.dayCounter,
            returnType=EquityReturnType_Total)

        self.assertGreater(len(leg), 0)

    def testEquityCouponWithNotionalReset(self):
        """Test EquityCoupon with notional reset enabled."""
        coupon = EquityCoupon(
            self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.fixingDays, self.equityIndex,
            self.dayCounter, EquityReturnType_Price,
            1.0, True, 100.0, 10000.0)

        self.assertTrue(coupon.notionalReset())
        self.assertAlmostEqual(coupon.inputInitialPrice(), 100.0, delta=1e-10)
        self.assertAlmostEqual(coupon.inputQuantity(), 10000.0, delta=1e-10)

    def testEquityCouponPricerVolSetters(self):
        """Test EquityCouponPricer volatility and correlation setters."""
        pricer = EquityCouponPricer()

        eqVol = BlackConstantVol(self.todayDate, self.eqCalendar, 0.20, self.dayCounter)
        eqVolHandle = BlackVolTermStructureHandle(eqVol)
        pricer.setEquityVolatility(eqVolHandle)

        fxVol = BlackConstantVol(self.todayDate, self.eqCalendar, 0.10, self.dayCounter)
        fxVolHandle = BlackVolTermStructureHandle(fxVol)
        pricer.setFxVolatility(fxVolHandle)


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(FXLinkedCashFlowTest,'test'))
    suite.addTest(unittest.makeSuite(FloatingRateFXLinkedNotionalCouponTest,'test'))
    suite.addTest(unittest.makeSuite(EquityCouponTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

