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


class IndexedCouponTest(unittest.TestCase):
    def setUp(self):
        """Set up an IndexedCoupon wrapping a FixedRateCoupon."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate

        self.dayCounter = Actual365Fixed()
        self.nominal = 1000000.0
        self.rate = 0.05
        self.startDate = Date(15, January, 2026)
        self.endDate = Date(15, April, 2026)
        self.paymentDate = Date(17, April, 2026)

        self.underlying = FixedRateCoupon(
            self.paymentDate, self.nominal,
            self.rate, self.dayCounter,
            self.startDate, self.endDate)

    def testIndexedCouponFixedMultiplier(self):
        """Test IndexedCoupon with a fixed multiplier."""
        qty = 2.0
        initialFixing = 1.5
        coupon = IndexedCoupon(self.underlying, qty, initialFixing)

        self.assertAlmostEqual(coupon.quantity(), qty, delta=1e-10)
        self.assertAlmostEqual(coupon.initialFixing(), initialFixing, delta=1e-10)
        expectedMultiplier = qty * initialFixing
        self.assertAlmostEqual(coupon.multiplier(), expectedMultiplier, delta=1e-10)

        underlyingAmount = self.underlying.amount()
        self.assertAlmostEqual(coupon.amount(), underlyingAmount * expectedMultiplier, delta=1e-6)

    def testIndexedCouponAccessors(self):
        """Test IndexedCoupon basic accessors."""
        qty = 1.0
        initialFixing = 2.0
        coupon = IndexedCoupon(self.underlying, qty, initialFixing)

        self.assertIsNotNone(coupon.underlying())
        self.assertEqual(coupon.dayCounter(), self.dayCounter)
        self.assertAlmostEqual(coupon.nominal(), self.nominal * qty * initialFixing, delta=1e-6)

    def testIndexWrappedCashFlowFixedMultiplier(self):
        """Test IndexWrappedCashFlow with a fixed multiplier."""
        qty = 3.0
        initialFixing = 1.25
        wrapped = IndexWrappedCashFlow(self.underlying, qty, initialFixing)

        self.assertEqual(wrapped.date(), self.paymentDate)
        expectedMultiplier = qty * initialFixing
        self.assertAlmostEqual(wrapped.multiplier(), expectedMultiplier, delta=1e-10)
        self.assertAlmostEqual(wrapped.amount(), self.underlying.amount() * expectedMultiplier, delta=1e-6)

    def testIndexedCouponLegBuilder(self):
        """Test IndexedCouponLeg builder produces a non-empty Leg."""
        calendar = TARGET()
        schedule = Schedule(
            self.startDate, Date(15, January, 2027),
            Period(3, Months), calendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)

        fixedLeg = FixedRateLeg(schedule, self.dayCounter, [self.nominal], [self.rate])

        flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        ytsHandle = RelinkableYieldTermStructureHandle(flatForward)
        eqSpot = QuoteHandle(SimpleQuote(100.0))
        eqIndex = EquityIndex2("EQ-IDX", calendar, USDCurrency(), eqSpot, ytsHandle, ytsHandle)

        leg = IndexedCouponLeg(
            underlyingLeg=fixedLeg,
            qty=1.0,
            index=eqIndex,
            initialFixing=100.0,
            fixingCalendar=calendar)

        self.assertGreater(len(leg), 0)

    def testUnpackIndexedCoupon(self):
        """Test unpackIndexedCoupon returns the underlying coupon."""
        qty = 2.0
        initialFixing = 1.5
        coupon = IndexedCoupon(self.underlying, qty, initialFixing)

        unpacked = unpackIndexedCoupon(coupon)
        self.assertIsNotNone(unpacked)

    def testGetMultiplier(self):
        """Test getIndexedCouponOrCashFlowMultiplier returns correct value."""
        qty = 2.0
        initialFixing = 1.5
        coupon = IndexedCoupon(self.underlying, qty, initialFixing)

        multiplier = getIndexedCouponOrCashFlowMultiplier(coupon)
        self.assertAlmostEqual(multiplier, qty * initialFixing, delta=1e-10)


class BondTRSCashFlowTest(unittest.TestCase):
    def setUp(self):
        """Set up a BondTRSCashFlow using a BondIndex.

        evaluationDate is set AFTER both fixing dates so they become historical
        and index.fixing() uses stored fixings rather than trying to forecast
        (which would require an actual Bond object attached to the BondIndex).
        """
        self.todayDate = Date(1, May, 2026)
        Settings.instance().evaluationDate = self.todayDate

        self.calendar = UnitedStates(UnitedStates.NYSE)
        self.dayCounter = Actual365Fixed()
        self.flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        self.discountCurve = RelinkableYieldTermStructureHandle(self.flatForward)

        # BondIndex with no bond object; only historical fixings will be used
        self.bondIndex = BondIndex(
            "BOND-TEST", False, True,
            self.calendar, None,
            self.discountCurve)

        self.fixingStartDate = Date(15, January, 2026)
        self.fixingEndDate = Date(15, April, 2026)
        self.paymentDate = Date(17, April, 2026)
        self.bondNotional = 1000000.0

        # Relative clean prices: par at start, +2% gain at end
        self.bondIndex.addFixing(self.fixingStartDate, 1.0)
        self.bondIndex.addFixing(self.fixingEndDate, 1.02)

    def testBondTRSCashFlowConstruction(self):
        """Test BondTRSCashFlow constructs and basic accessors work."""
        cf = BondTRSCashFlow(
            self.paymentDate,
            self.fixingStartDate,
            self.fixingEndDate,
            self.bondNotional,
            self.bondIndex)

        self.assertEqual(cf.date(), self.paymentDate)
        self.assertAlmostEqual(cf.notional(), self.bondNotional, delta=1e-10)
        self.assertEqual(cf.fixingStartDate(), self.fixingStartDate)
        self.assertEqual(cf.fixingEndDate(), self.fixingEndDate)

    def testBondTRSCashFlowAmount(self):
        """Test BondTRSCashFlow.amount() computes total return correctly."""
        cf = BondTRSCashFlow(
            self.paymentDate,
            self.fixingStartDate,
            self.fixingEndDate,
            self.bondNotional,
            self.bondIndex)

        # amount = notional * (endPrice - startPrice) = 1e6 * (1.02 - 1.0) = 20000
        self.assertAlmostEqual(cf.amount(), 20000.0, delta=1e-4)

    def testBondTRSCashFlowWithInitialPrice(self):
        """Test BondTRSCashFlow construction with explicit initialPrice."""
        cf = BondTRSCashFlow(
            self.paymentDate,
            self.fixingStartDate,
            self.fixingEndDate,
            self.bondNotional,
            self.bondIndex,
            1.0)  # initialPrice

        # Basic accessors still work; amount() is not tested here because
        # with initialPrice set, assetStart() calls notional(date) which
        # requires a real Bond object attached to BondIndex.
        self.assertEqual(cf.date(), self.paymentDate)
        self.assertAlmostEqual(cf.notional(), self.bondNotional, delta=1e-10)
        self.assertAlmostEqual(cf.initialPrice(), 1.0, delta=1e-10)

    def testBondTRSCashFlowSetFixingStartDate(self):
        """Test setFixingStartDate updates the fixing start date."""
        cf = BondTRSCashFlow(
            self.paymentDate,
            self.fixingStartDate,
            self.fixingEndDate,
            self.bondNotional,
            self.bondIndex)

        # Feb 2, 2026 is a Monday (valid NYSE business day)
        newStart = Date(2, February, 2026)
        self.bondIndex.addFixing(newStart, 1.01)
        cf.setFixingStartDate(newStart)
        self.assertEqual(cf.fixingStartDate(), newStart)

    def testBondTRSLegBuilder(self):
        """Test BondTRSLeg builder produces a Leg with correct length."""
        valuationDates = [
            Date(15, January, 2026),
            Date(15, April, 2026),
            Date(15, July, 2026)]
        paymentDates = [
            Date(17, April, 2026),
            Date(17, July, 2026)]

        self.bondIndex.addFixing(Date(15, July, 2026), 1.03)

        leg = BondTRSLeg(
            valuationDates=valuationDates,
            paymentDates=paymentDates,
            bondNotional=self.bondNotional,
            index=self.bondIndex,
            initialPrice=1.0)

        # leg has len(valuationDates) - 1 cashflows
        self.assertEqual(len(leg), 2)


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(FXLinkedCashFlowTest,'test'))
    suite.addTest(unittest.makeSuite(FloatingRateFXLinkedNotionalCouponTest,'test'))
    suite.addTest(unittest.makeSuite(CommodityIndexedAverageCashFlowTest,'test'))
    suite.addTest(unittest.makeSuite(EquityCouponTest,'test'))
    suite.addTest(unittest.makeSuite(IndexedCouponTest,'test'))
    suite.addTest(unittest.makeSuite(BondTRSCashFlowTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

