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

        
class CommodityIndexedAverageCashFlowTest(unittest.TestCase):
    def setUp(self):
        """Set up a CommodityIndexedAverageCashFlow with a CommoditySpotIndex."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate

        self.commName = "COMMODITY_WTI"
        self.commCalendar = UnitedStates(UnitedStates.NYSE)

        # Use two-arg constructor (no price curve); fixings will supply prices
        self.commodityIndex = CommoditySpotIndex(
            self.commName, self.commCalendar)

        self.startDate = Date(2, February, 2026)
        self.endDate = Date(2, March, 2026)
        self.paymentDate = Date(3, March, 2026)
        self.quantity = 1000.0

    def testExplicitPaymentDateConstructor(self):
        """Test CommodityIndexedAverageCashFlow with explicit payment date."""
        cf = CommodityIndexedAverageCashFlow(
            self.quantity, self.startDate, self.endDate,
            self.paymentDate, self.commodityIndex)

        self.assertEqual(cf.date(), self.paymentDate)
        self.assertEqual(cf.startDate(), self.startDate)
        self.assertEqual(cf.endDate(), self.endDate)
        self.assertAlmostEqual(cf.periodQuantity(), self.quantity, delta=1e-10)

    def testDeducedPaymentDateConstructor(self):
        """Test CommodityIndexedAverageCashFlow with deduced payment date."""
        cf = CommodityIndexedAverageCashFlow(
            self.quantity, self.startDate, self.endDate,
            0, self.commCalendar, Following,
            self.commodityIndex)

        self.assertEqual(cf.startDate(), self.startDate)
        self.assertEqual(cf.endDate(), self.endDate)
        self.assertAlmostEqual(cf.periodQuantity(), self.quantity, delta=1e-10)
        # Payment date should be on or after end date
        self.assertGreaterEqual(cf.date(), self.endDate)

    def testAccessors(self):
        """Test CommodityIndexedAverageCashFlow accessor methods."""
        cf = CommodityIndexedAverageCashFlow(
            self.quantity, self.startDate, self.endDate,
            self.paymentDate, self.commodityIndex,
            self.commCalendar, 0.5, 1.0, False, 0, 0)

        self.assertEqual(cf.deliveryDateRoll(), 0)
        self.assertEqual(cf.futureMonthOffset(), 0)
        self.assertTrue(cf.useBusinessDays())
        self.assertIsNotNone(cf.index())

    def testCommodityIndexedAverageLeg(self):
        """Test CommodityIndexedAverageLeg builder produces a non-empty Leg."""
        schedule = Schedule(
            self.startDate, Date(2, February, 2027),
            Period(1, Months), self.commCalendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)

        leg = CommodityIndexedAverageLeg(
            schedule=schedule,
            index=self.commodityIndex,
            quantities=[self.quantity],
            paymentCalendar=self.commCalendar,
            pricingCalendar=self.commCalendar)

        self.assertGreater(len(leg), 0)


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(FXLinkedCashFlowTest,'test'))
    suite.addTest(unittest.makeSuite(FloatingRateFXLinkedNotionalCouponTest,'test'))
    suite.addTest(unittest.makeSuite(CommodityIndexedAverageCashFlowTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

