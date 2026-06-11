"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.
"""

from ORE import *
import ORE
import unittest


def _build_inflation_index_with_curve(today, baseCPI, observationLag, inflRate, dayCounter, calendar, ytsHandle):
    """Helper: build a UKRPI with a PiecewiseZeroInflation curve."""
    baseIndex = UKRPI()
    baseIndex.addFixing(Date(1, October, 2025), baseCPI)

    baseDate = Date(1, October, 2025)

    zeroHelper = ZeroCouponInflationSwapHelper(
        QuoteHandle(SimpleQuote(inflRate)),
        observationLag,
        Date(15, January, 2028),
        calendar,
        ModifiedFollowing,
        dayCounter,
        baseIndex,
        CPI.Flat,
        ytsHandle)

    inflCurve = PiecewiseZeroInflation(
        today, baseDate,
        Monthly,
        dayCounter, [zeroHelper])

    inflHandle = RelinkableZeroInflationTermStructureHandle(inflCurve)
    linkedIndex = UKRPI(inflHandle)
    linkedIndex.addFixing(Date(1, October, 2025), baseCPI)
    return linkedIndex, inflCurve


class QLECPICouponTest(unittest.TestCase):
    def setUp(self):
        """Set up a QLECPICoupon with a ZeroInflationIndex and flat curve."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate
        self.dayCounter = Actual365Fixed()
        self.calendar = UnitedKingdom()
        self.baseCPI = 100.0
        self.observationLag = Period(3, Months)

        self.flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        self.ytsHandle = RelinkableYieldTermStructureHandle(self.flatForward)

        self.inflIndexLinked, self._inflCurve = _build_inflation_index_with_curve(
            self.todayDate, self.baseCPI, self.observationLag, 0.025,
            self.dayCounter, self.calendar, self.ytsHandle)

        self.startDate = Date(15, January, 2026)
        self.endDate = Date(15, January, 2027)
        self.paymentDate = Date(17, January, 2027)
        self.nominal = 1000000.0
        self.fixedRate = 1.0

    def testQLECPICouponConstruction(self):
        """Test QLECPICoupon construction and basic accessors."""
        coupon = QLECPICoupon(
            self.baseCPI, self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.inflIndexLinked, self.observationLag,
            CPI.Flat, self.dayCounter, self.fixedRate)

        self.assertEqual(coupon.date(), self.paymentDate)
        self.assertAlmostEqual(coupon.nominal(), self.nominal, delta=1e-10)
        self.assertFalse(coupon.subtractInflationNotional())

    def testQLECPICouponSubtractInflationNominal(self):
        """Test QLECPICoupon subtractInflationNominal flag."""
        coupon = QLECPICoupon(
            self.baseCPI, self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.inflIndexLinked, self.observationLag,
            CPI.Flat, self.dayCounter, self.fixedRate,
            Date(), Date(), Date(), True)

        self.assertTrue(coupon.subtractInflationNotional())

    def testQLECPICouponDefaultSubtractFlag(self):
        """Test that subtractInflationNominal defaults to False."""
        coupon = QLECPICoupon(
            self.baseCPI, self.paymentDate, self.nominal,
            self.startDate, self.endDate,
            self.inflIndexLinked, self.observationLag,
            CPI.Flat, self.dayCounter, self.fixedRate)

        self.assertFalse(coupon.subtractInflationNotional())


class CappedFlooredCPICashFlowTest(unittest.TestCase):
    def setUp(self):
        """Set up a CPICashFlow for capped/floored tests."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate
        self.baseCPI = 100.0
        self.observationLag = Period(3, Months)

        self.inflIndex = UKRPI()
        self.inflIndex.addFixing(Date(1, October, 2025), self.baseCPI)

        self.baseDate = Date(1, October, 2025)
        self.observationDate = Date(1, October, 2026)
        self.paymentDate = Date(17, January, 2027)

        self.underlying = CPICashFlow(
            1000000.0,
            self.inflIndex,
            self.baseDate,
            self.baseCPI,
            self.observationDate,
            self.observationLag,
            CPI.Flat,
            self.paymentDate,
            True)

    def testCappedConstruction(self):
        """Test CappedFlooredCPICashFlow construction with a cap."""
        capped = CappedFlooredCPICashFlow(
            self.underlying, self.baseDate, self.observationLag, 0.03)
        self.assertTrue(capped.isCapped())
        self.assertFalse(capped.isFloored())
        self.assertIsNotNone(capped.underlying())

    def testFlooredConstruction(self):
        """Test CappedFlooredCPICashFlow construction with a floor."""
        floored = CappedFlooredCPICashFlow(
            self.underlying, self.baseDate, self.observationLag,
            nullDouble(), 0.01)
        self.assertFalse(floored.isCapped())
        self.assertTrue(floored.isFloored())

    def testCappedAndFloored(self):
        """Test CappedFlooredCPICashFlow construction with both cap and floor."""
        collar = CappedFlooredCPICashFlow(
            self.underlying, self.baseDate, self.observationLag,
            0.05, 0.01)
        self.assertTrue(collar.isCapped())
        self.assertTrue(collar.isFloored())


class InflationPricerTest(unittest.TestCase):
    def testInflationCashFlowPricerConstruction(self):
        """Test InflationCashFlowPricer default construction."""
        pricer = InflationCashFlowPricer()
        self.assertIsNotNone(pricer)

    def testBlackCPICashFlowPricerConstruction(self):
        """Test BlackCPICashFlowPricer default construction."""
        pricer = BlackCPICashFlowPricer()
        self.assertIsNotNone(pricer)

    def testBachelierCPICashFlowPricerConstruction(self):
        """Test BachelierCPICashFlowPricer default construction."""
        pricer = BachelierCPICashFlowPricer()
        self.assertIsNotNone(pricer)

    def testCappedFlooredCPICouponPricerConstruction(self):
        """Test CappedFlooredCPICouponPricer default construction."""
        pricer = CappedFlooredCPICouponPricer()
        self.assertIsNotNone(pricer)

    def testBlackCPICouponPricerConstruction(self):
        """Test BlackCPICouponPricer default construction."""
        pricer = BlackCPICouponPricer()
        self.assertIsNotNone(pricer)

    def testBachelierCPICouponPricerConstruction(self):
        """Test BachelierCPICouponPricer default construction."""
        pricer = BachelierCPICouponPricer()
        self.assertIsNotNone(pricer)


class QLECPILegTest(unittest.TestCase):
    def setUp(self):
        """Set up infrastructure for QLECPILeg tests."""
        self.todayDate = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.todayDate
        self.dayCounter = Actual365Fixed()
        self.calendar = UnitedKingdom()
        self.baseCPI = 100.0
        self.observationLag = Period(3, Months)

        self.flatForward = FlatForward(self.todayDate, 0.03, self.dayCounter)
        self.ytsHandle = RelinkableYieldTermStructureHandle(self.flatForward)

        self.inflIndexLinked, self._inflCurve = _build_inflation_index_with_curve(
            self.todayDate, self.baseCPI, self.observationLag, 0.025,
            self.dayCounter, self.calendar, self.ytsHandle)

        self.schedule = Schedule(
            Date(15, January, 2026), Date(15, January, 2029),
            Period(6, Months), self.calendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)

    def testQLECPILegConstruction(self):
        """Test QLECPILeg builder produces a non-empty leg."""
        leg = QLECPILeg(
            schedule=self.schedule,
            index=self.inflIndexLinked,
            rateCurve=self.ytsHandle,
            baseCPI=self.baseCPI,
            observationLag=self.observationLag,
            notionals=[1000000.0],
            fixedRates=[1.0],
            paymentDayCounter=self.dayCounter,
            paymentCalendar=self.calendar,
            observationInterpolation=CPI.Flat)

        self.assertGreater(len(leg), 0)

    def testQLECPILegWithSubtractInflationNominal(self):
        """Test QLECPILeg with subtractInflationNominal=True."""
        leg = QLECPILeg(
            schedule=self.schedule,
            index=self.inflIndexLinked,
            rateCurve=self.ytsHandle,
            baseCPI=self.baseCPI,
            observationLag=self.observationLag,
            notionals=[1000000.0],
            fixedRates=[1.0],
            paymentDayCounter=self.dayCounter,
            paymentCalendar=self.calendar,
            observationInterpolation=CPI.Flat,
            subtractInflationNominal=True)

        self.assertGreater(len(leg), 0)

    def testQLECPILegWithCapsFloors(self):
        """Test QLECPILeg with caps and floors."""
        leg = QLECPILeg(
            schedule=self.schedule,
            index=self.inflIndexLinked,
            rateCurve=self.ytsHandle,
            baseCPI=self.baseCPI,
            observationLag=self.observationLag,
            notionals=[1000000.0],
            fixedRates=[1.0],
            paymentDayCounter=self.dayCounter,
            paymentCalendar=self.calendar,
            observationInterpolation=CPI.Flat,
            caps=[0.05],
            floors=[0.01])

        self.assertGreater(len(leg), 0)


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(QLECPICouponTest, 'test'))
    suite.addTest(unittest.makeSuite(CappedFlooredCPICashFlowTest, 'test'))
    suite.addTest(unittest.makeSuite(InflationPricerTest, 'test'))
    suite.addTest(unittest.makeSuite(QLECPILegTest, 'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()
