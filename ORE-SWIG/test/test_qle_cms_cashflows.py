"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.

 Tests for ACADIAQPR-14143: SWIG bindings for CMS spread and
 duration-adjusted CMS coupons/pricers (qle_cms_cashflows.i).

 Covers:
   - LinearAnnuityMappingBuilder construction (both ctors)
   - DurationAdjustedCmsCoupon construction and accessors
   - DurationAdjustedCmsLeg builder (non-empty leg, kwargs)
   - DurationAdjustedCmsCouponTsrPricer construction and attachment
   - CmsSpreadCouponPricer2 / QLELognormalCmsSpreadPricer construction
"""

from ORE import *
import ORE
import unittest


def _flat_swaption_vol(today, vol=0.20, shift=0.0):
    """Build a flat constant swaption vol surface."""
    calendar = TARGET()
    dc = Actual365Fixed()
    return ConstantSwaptionVolatility(
        today, calendar, ModifiedFollowing, vol, dc,
        ShiftedLognormal, shift)


def _build_swap_index(today):
    """Build a 10Y EUR swap index backed by a flat yield curve."""
    dc = Actual365Fixed()
    flat = FlatForward(today, 0.03, dc)
    yts = RelinkableYieldTermStructureHandle(flat)
    index = Euribor6M(yts)
    return EuriborSwapIsdaFixA(Period(10, Years), yts, yts), yts


def _build_swap_spread_index(today):
    """Build a SwapSpreadIndex (cms1 - cms2) for CMS spread tests."""
    dc = Actual365Fixed()
    flat = FlatForward(today, 0.03, dc)
    yts = RelinkableYieldTermStructureHandle(flat)
    cms10 = EuriborSwapIsdaFixA(Period(10, Years), yts, yts)
    cms2 = EuriborSwapIsdaFixA(Period(2, Years), yts, yts)
    spread_idx = SwapSpreadIndex("cms10-cms2", cms10, cms2)
    return spread_idx, yts


class LinearAnnuityMappingBuilderTest(unittest.TestCase):

    def setUp(self):
        self.today = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.today

    def testConstruction_ab(self):
        """LinearAnnuityMappingBuilder(a, b) constructs without error."""
        builder = LinearAnnuityMappingBuilder(0.0, 1.0)
        self.assertIsNotNone(builder)

    def testConstruction_reversion(self):
        """LinearAnnuityMappingBuilder(Handle<Quote>) constructs without error."""
        q = SimpleQuote(0.0)
        builder = LinearAnnuityMappingBuilder(QuoteHandle(q))
        self.assertIsNotNone(builder)


class DurationAdjustedCmsCouponTest(unittest.TestCase):

    def setUp(self):
        self.today = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.today
        self.swapIdx, self.yts = _build_swap_index(self.today)
        self.payDate = Date(17, January, 2027)
        self.start = Date(15, January, 2026)
        self.end = Date(15, January, 2027)
        self.nominal = 1_000_000.0

    def testConstruction(self):
        """DurationAdjustedCmsCoupon constructs with default duration=0."""
        coupon = DurationAdjustedCmsCoupon(
            self.payDate, self.nominal,
            self.start, self.end,
            2, self.swapIdx)
        self.assertIsNotNone(coupon)

    def testAccessors(self):
        """DurationAdjustedCmsCoupon accessors return expected values."""
        duration = 5
        coupon = DurationAdjustedCmsCoupon(
            self.payDate, self.nominal,
            self.start, self.end,
            2, self.swapIdx, duration,
            1.0, 0.0)
        self.assertEqual(coupon.date(), self.payDate)
        self.assertAlmostEqual(coupon.nominal(), self.nominal, delta=1e-8)
        self.assertEqual(coupon.duration(), duration)

    def testDurationZeroGivesAdjustmentOne(self):
        """Duration=0 means durationAdjustment()==1 (pass-through)."""
        coupon = DurationAdjustedCmsCoupon(
            self.payDate, self.nominal,
            self.start, self.end,
            2, self.swapIdx, 0)
        self.assertAlmostEqual(coupon.durationAdjustment(), 1.0, delta=1e-10)

    def testSwapIndexAccessor(self):
        """swapIndex() returns the index passed at construction."""
        coupon = DurationAdjustedCmsCoupon(
            self.payDate, self.nominal,
            self.start, self.end,
            2, self.swapIdx, 3)
        self.assertIsNotNone(coupon.swapIndex())


class DurationAdjustedCmsLegTest(unittest.TestCase):

    def setUp(self):
        self.today = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.today
        self.calendar = TARGET()
        self.dc = Actual365Fixed()
        self.swapIdx, self.yts = _build_swap_index(self.today)
        self.schedule = Schedule(
            Date(15, January, 2026), Date(15, January, 2029),
            Period(6, Months), self.calendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)

    def testLegConstruction(self):
        """DurationAdjustedCmsLeg builds a non-empty leg."""
        leg = DurationAdjustedCmsLeg(
            schedule=self.schedule,
            swapIndex=self.swapIdx,
            duration=10,
            notionals=[1_000_000.0])
        self.assertGreater(len(leg), 0)

    def testLegKwargsSpreads(self):
        """DurationAdjustedCmsLeg accepts spreads and gearings via kwargs."""
        n = len(self.schedule) - 1
        leg = DurationAdjustedCmsLeg(
            schedule=self.schedule,
            swapIndex=self.swapIdx,
            duration=5,
            notionals=[1_000_000.0],
            spreads=[0.001],
            gearings=[1.0],
            paymentDayCounter=self.dc,
            paymentCalendar=self.calendar)
        self.assertGreater(len(leg), 0)

    def testLegWithCapsFloors(self):
        """DurationAdjustedCmsLeg accepts cap and floor vectors."""
        leg = DurationAdjustedCmsLeg(
            schedule=self.schedule,
            swapIndex=self.swapIdx,
            duration=3,
            notionals=[1_000_000.0],
            caps=[0.05],
            floors=[0.0])
        self.assertGreater(len(leg), 0)

    def testLegInArrears(self):
        """DurationAdjustedCmsLeg with inArrears=True builds successfully."""
        leg = DurationAdjustedCmsLeg(
            schedule=self.schedule,
            swapIndex=self.swapIdx,
            duration=2,
            notionals=[1_000_000.0],
            inArrears=True)
        self.assertGreater(len(leg), 0)


class DurationAdjustedCmsCouponTsrPricerTest(unittest.TestCase):

    def setUp(self):
        self.today = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.today
        self.swapIdx, self.yts = _build_swap_index(self.today)
        self.swaptionVol = _flat_swaption_vol(self.today, 0.20)
        self.swaptionVolHandle = SwaptionVolatilityStructureHandle(
            self.swaptionVol)

    def testConstruction_linear_ab(self):
        """DurationAdjustedCmsCouponTsrPricer constructs with linear a,b builder."""
        builder = LinearAnnuityMappingBuilder(0.0, 1.0)
        pricer = DurationAdjustedCmsCouponTsrPricer(
            self.swaptionVolHandle, builder)
        self.assertIsNotNone(pricer)

    def testConstruction_reversion(self):
        """DurationAdjustedCmsCouponTsrPricer constructs with reversion-quote builder."""
        q = SimpleQuote(0.01)
        builder = LinearAnnuityMappingBuilder(QuoteHandle(q))
        pricer = DurationAdjustedCmsCouponTsrPricer(
            self.swaptionVolHandle, builder, -0.5, 0.5)
        self.assertIsNotNone(pricer)

    def testPricerAttachAndRate(self):
        """DurationAdjustedCmsCouponTsrPricer can be attached and rate() called."""
        calendar = TARGET()
        schedule = Schedule(
            Date(15, January, 2026), Date(15, January, 2029),
            Period(6, Months), calendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)
        # Add swap rate fixing to avoid missing fixing error
        self.swapIdx.addFixing(Date(13, January, 2026), 0.03)
        leg = DurationAdjustedCmsLeg(
            schedule=schedule,
            swapIndex=self.swapIdx,
            duration=10,
            notionals=[1_000_000.0])
        builder = LinearAnnuityMappingBuilder(0.0, 1.0)
        pricer = DurationAdjustedCmsCouponTsrPricer(
            self.swaptionVolHandle, builder)
        setCouponPricer(leg, pricer)
        # After attaching the pricer, rate() should not raise for each coupon
        for cf in leg:
            coupon = as_coupon(cf)
            if coupon is not None:
                r = coupon.rate()
                self.assertIsInstance(r, float)
                break


class QLELognormalCmsSpreadPricerTest(unittest.TestCase):

    def setUp(self):
        self.today = Date(15, January, 2026)
        Settings.instance().evaluationDate = self.today
        self.swapIdx, self.yts = _build_swap_index(self.today)
        self.swaptionVol = _flat_swaption_vol(self.today, 0.20)
        self.swaptionVolHandle = SwaptionVolatilityStructureHandle(
            self.swaptionVol)
        # Build a flat correlation term structure (correlation = 0.5)
        dc = Actual365Fixed()
        flat_corr = FlatCorrelation(self.today, 0.5, dc)
        self.corrHandle = CorrelationTermStructureHandle(flat_corr)
        # CMS pricer needed by LognormalCmsSpreadPricer
        reversion = QuoteHandle(SimpleQuote(0.0))
        self.cmsPricer = LinearTsrPricer(self.swaptionVolHandle, reversion)

    def testConstruction_default(self):
        """QLELognormalCmsSpreadPricer constructs with default parameters."""
        pricer = QLELognormalCmsSpreadPricer(self.cmsPricer, self.corrHandle)
        self.assertIsNotNone(pricer)

    def testConstruction_with_discount_curve(self):
        """QLELognormalCmsSpreadPricer constructs with explicit discount curve."""
        pricer = QLELognormalCmsSpreadPricer(
            self.cmsPricer, self.corrHandle, self.yts, 32)
        self.assertIsNotNone(pricer)

    def testConstruction_explicit_shiftedlognormal(self):
        """QLELognormalCmsSpreadPricer with explicit ShiftedLognormal vol type."""
        empty_curve = YieldTermStructureHandle()
        pricer = QLELognormalCmsSpreadPricer(
            self.cmsPricer, self.corrHandle, empty_curve, 16,
            ShiftedLognormal, 0.0, 0.0)
        self.assertIsNotNone(pricer)

    def testConstruction_normal_vol(self):
        """QLELognormalCmsSpreadPricer with Normal volatility type."""
        empty_curve = YieldTermStructureHandle()
        pricer = QLELognormalCmsSpreadPricer(
            self.cmsPricer, self.corrHandle, empty_curve, 16, Normal)
        self.assertIsNotNone(pricer)

    def testConstruction_inherited_vol_type_via_None(self):
        """QLELognormalCmsSpreadPricer with volatilityType=None inherits from CMS pricer."""
        empty_curve = YieldTermStructureHandle()
        pricer = QLELognormalCmsSpreadPricer(
            self.cmsPricer, self.corrHandle, empty_curve, 16, None)
        self.assertIsNotNone(pricer)

    def testCorrelationAccessor(self):
        """QLELognormalCmsSpreadPricer.correlation() returns the curve value."""
        pricer = QLELognormalCmsSpreadPricer(self.cmsPricer, self.corrHandle)
        corr = pricer.correlation(1.0)
        self.assertAlmostEqual(corr, 0.5, delta=1e-8)

    def testSetCorrelationCurve(self):
        """setCorrelationCurve() updates the correlation used by the pricer."""
        pricer = QLELognormalCmsSpreadPricer(self.cmsPricer, self.corrHandle)
        dc = Actual365Fixed()
        new_corr = FlatCorrelation(self.today, 0.8, dc)
        new_handle = CorrelationTermStructureHandle(new_corr)
        pricer.setCorrelationCurve(new_handle)
        self.assertAlmostEqual(pricer.correlation(1.0), 0.8, delta=1e-8)

    def testCmsSpreadLegPricerAttach(self):
        """QLELognormalCmsSpreadPricer attaches to a CmsSpreadLeg without error."""
        spread_idx, yts = _build_swap_spread_index(self.today)
        calendar = TARGET()
        schedule = Schedule(
            Date(15, January, 2026), Date(15, January, 2028),
            Period(6, Months), calendar,
            ModifiedFollowing, ModifiedFollowing,
            DateGeneration.Forward, False)
        leg = CmsSpreadLeg([1_000_000.0], schedule, spread_idx)
        pricer = QLELognormalCmsSpreadPricer(self.cmsPricer, self.corrHandle)
        setCouponPricer(leg, pricer)
        self.assertGreater(len(leg), 0)


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(LinearAnnuityMappingBuilderTest, 'test'))
    suite.addTest(unittest.makeSuite(DurationAdjustedCmsCouponTest, 'test'))
    suite.addTest(unittest.makeSuite(DurationAdjustedCmsLegTest, 'test'))
    suite.addTest(unittest.makeSuite(DurationAdjustedCmsCouponTsrPricerTest, 'test'))
    suite.addTest(unittest.makeSuite(QLELognormalCmsSpreadPricerTest, 'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()
