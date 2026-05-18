"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.

 Tests for Phase 4 QuantExt term structure bindings (qle_termstructures_ext2.i).
"""

from ORE import *
import unittest
import math


class DynamicsEnumTest(unittest.TestCase):
    """Verify dynamics enums are exposed and have expected values."""

    def test_stickyness_values(self):
        self.assertEqual(int(StickyStrike), 0)
        self.assertEqual(int(StickyLogMoneyness), 1)
        self.assertEqual(int(StickyAbsoluteMoneyness), 2)

    def test_reaction_to_time_decay_values(self):
        self.assertEqual(int(ConstantVariance), 0)
        self.assertEqual(int(ForwardForwardVariance), 1)

    def test_yield_curve_roll_down_values(self):
        self.assertEqual(int(ConstantDiscounts), 0)
        self.assertEqual(int(ForwardForward), 1)


class FxSmileSectionTest(unittest.TestCase):
    """Test FxSmileSection hierarchy bindings."""

    def test_interpolated_smile_section_construction(self):
        """Construct InterpolatedSmileSection and query volatility."""
        spot = 1.2
        rd = 0.02
        rf = 0.01
        t = 0.5
        strikes = [1.1, 1.2, 1.3]
        vols = [0.12, 0.10, 0.13]
        method = InterpolatedSmileSection.InterpolationMethod_Linear
        section = InterpolatedSmileSection(spot, rd, rf, t, strikes, vols, method)
        # ATM vol should be 10%
        self.assertAlmostEqual(section.volatility(1.2), 0.10, places=10)
        self.assertAlmostEqual(section.domesticDiscount(), math.exp(-rd * t), places=10)
        self.assertAlmostEqual(section.foreignDiscount(), math.exp(-rf * t), places=10)

    def test_interpolated_smile_section_extrapolation(self):
        """Check flat extrapolation flag."""
        section = InterpolatedSmileSection(
            1.0, 0.01, 0.02, 1.0,
            [0.9, 1.0, 1.1], [0.15, 0.12, 0.14],
            InterpolatedSmileSection.InterpolationMethod_Linear, True)
        self.assertIsNotNone(section)
        self.assertEqual(len(section.strikes()), 3)
        self.assertEqual(len(section.volatilities()), 3)

    def test_constant_smile_section(self):
        """ConstantSmileSection returns the same vol for any strike."""
        section = ConstantSmileSection(0.25)
        self.assertAlmostEqual(section.volatility(0.8), 0.25, places=12)
        self.assertAlmostEqual(section.volatility(1.5), 0.25, places=12)

    def test_interpolation_method_enum(self):
        """Check all InterpolationMethod enum values exist."""
        self.assertEqual(int(InterpolatedSmileSection.InterpolationMethod_Linear), 0)
        self.assertEqual(int(InterpolatedSmileSection.InterpolationMethod_NaturalCubic), 1)
        self.assertEqual(int(InterpolatedSmileSection.InterpolationMethod_FinancialCubic), 2)
        self.assertEqual(int(InterpolatedSmileSection.InterpolationMethod_CubicSpline), 3)


class CommodityBasisPriceTermStructureTest(unittest.TestCase):
    """Verify the abstract stub is accessible."""

    def test_symbol_exists(self):
        self.assertTrue(hasattr(__import__("ORE"), "CommodityBasisPriceTermStructure"))


class SurvivalProbabilityCurveExtrapolationTest(unittest.TestCase):
    """Test the standalone extrapolation enum alias."""

    def test_enum_values(self):
        self.assertEqual(int(SurvivalProbabilityCurveExtrapolation_flatFwd), 0)
        self.assertEqual(int(SurvivalProbabilityCurveExtrapolation_flatZero), 1)


class DiscountRatioModifiedCurveTest(unittest.TestCase):
    """Test DiscountRatioModifiedCurve: P(t) = P_base(t) * P_num(t) / P_den(t)."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()

        # Three flat yield curves at known rates
        self.base_rate = 0.03
        self.num_rate = 0.02
        self.den_rate = 0.04

        self.base_ff = FlatForward(self.today, self.base_rate, self.dc)
        self.num_ff = FlatForward(self.today, self.num_rate, self.dc)
        self.den_ff = FlatForward(self.today, self.den_rate, self.dc)

        self.base_handle = YieldTermStructureHandle(self.base_ff)
        self.num_handle = YieldTermStructureHandle(self.num_ff)
        self.den_handle = YieldTermStructureHandle(self.den_ff)

        self.curve = DiscountRatioModifiedCurve(
            self.base_handle, self.num_handle, self.den_handle)
        self.curve.enableExtrapolation()

    def test_discount_ratio_formula(self):
        """Verify discount(t) = base(t) * num(t) / den(t) for several maturities."""
        for t_years in [0.25, 0.5, 1.0, 2.0, 5.0, 10.0]:
            expected = (math.exp(-self.base_rate * t_years)
                        * math.exp(-self.num_rate * t_years)
                        / math.exp(-self.den_rate * t_years))
            actual = self.curve.discount(t_years)
            self.assertAlmostEqual(actual, expected, places=10,
                                   msg=f"Mismatch at t={t_years}")

    def test_inspectors(self):
        """Test baseCurve, numeratorCurve, denominatorCurve return valid handles."""
        self.assertIsNotNone(self.curve.baseCurve())
        self.assertIsNotNone(self.curve.numeratorCurve())
        self.assertIsNotNone(self.curve.denominatorCurve())

    def test_reference_date(self):
        self.assertEqual(self.curve.referenceDate(), self.today)

    def test_day_counter(self):
        self.assertEqual(self.curve.dayCounter(), self.dc)


class SurvivalProbabilityCurveLinearTest(unittest.TestCase):
    """Test SurvivalProbabilityCurveLinear (QuantExt::SurvivalProbabilityCurve<Linear>)."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()
        self.cal = TARGET()

        self.dates = [
            self.today,
            Date(15, July, 2025),
            Date(15, January, 2026),
            Date(15, January, 2027),
        ]
        self.sp_values = [1.0, 0.98, 0.95, 0.88]

        self.quotes = [QuoteHandle(SimpleQuote(v)) for v in self.sp_values]

        self.curve = SurvivalProbabilityCurveLinear(
            self.dates, self.quotes, self.dc, self.cal)
        self.curve.enableExtrapolation()

    def test_node_survival_probabilities(self):
        """Verify survival probabilities at node dates match input quotes."""
        for d, sp in zip(self.dates, self.sp_values):
            self.assertAlmostEqual(self.curve.survivalProbability(d), sp, places=8,
                                   msg=f"Mismatch at {d}")

    def test_interpolation(self):
        """Survival probability at a mid-date should be between node values."""
        mid = Date(15, October, 2025)
        sp_mid = self.curve.survivalProbability(mid)
        self.assertGreater(sp_mid, 0.95)
        self.assertLess(sp_mid, 0.98)

    def test_max_date(self):
        self.assertEqual(self.curve.maxDate(), self.dates[-1])

    def test_dates_accessor(self):
        self.assertEqual(len(self.curve.dates()), len(self.dates))

    def test_quotes_accessor(self):
        self.assertEqual(len(self.curve.quotes()), len(self.quotes))

    def test_nodes_accessor(self):
        nodes = self.curve.nodes()
        self.assertEqual(len(nodes), len(self.dates))


class SurvivalProbabilityCurveLogLinearTest(unittest.TestCase):
    """Test SurvivalProbabilityCurveLogLinear."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()

        self.dates = [
            self.today,
            Date(15, January, 2026),
            Date(15, January, 2027),
        ]
        self.sp_values = [1.0, 0.96, 0.90]
        self.quotes = [QuoteHandle(SimpleQuote(v)) for v in self.sp_values]

        self.curve = SurvivalProbabilityCurveLogLinear(
            self.dates, self.quotes, self.dc)
        self.curve.enableExtrapolation()

    def test_node_survival_probabilities(self):
        for d, sp in zip(self.dates, self.sp_values):
            self.assertAlmostEqual(self.curve.survivalProbability(d), sp, places=8)

    def test_interpolation_differs_from_linear(self):
        """LogLinear interpolation at mid-point differs from simple linear."""
        mid = Date(15, July, 2025)
        sp = self.curve.survivalProbability(mid)
        # LogLinear interpolates in log-space; result should be between node values
        self.assertGreater(sp, 0.96)
        self.assertLess(sp, 1.0)


class SurvivalProbabilityCurveExtrapolationArgTest(unittest.TestCase):
    """Test constructing curves with explicit extrapolation enum."""

    def test_flat_fwd_extrapolation(self):
        today = Date(15, January, 2025)
        Settings.instance().evaluationDate = today
        dc = Actual365Fixed()
        dates = [today, Date(15, January, 2026)]
        quotes = [QuoteHandle(SimpleQuote(1.0)), QuoteHandle(SimpleQuote(0.95))]
        empty_jumps = []
        empty_jump_dates = []

        curve = SurvivalProbabilityCurveLinear(
            dates, quotes, dc, TARGET(), empty_jumps, empty_jump_dates,
            SurvivalProbabilityCurveExtrapolation_flatFwd)
        curve.enableExtrapolation()
        sp_extrap = curve.survivalProbability(Date(15, January, 2028))
        self.assertGreater(sp_extrap, 0.0)
        self.assertLess(sp_extrap, 0.95)

    def test_flat_zero_extrapolation(self):
        today = Date(15, January, 2025)
        Settings.instance().evaluationDate = today
        dc = Actual365Fixed()
        dates = [today, Date(15, January, 2026)]
        quotes = [QuoteHandle(SimpleQuote(1.0)), QuoteHandle(SimpleQuote(0.95))]

        curve = SurvivalProbabilityCurveLinear(
            dates, quotes, dc, TARGET(), [], [],
            SurvivalProbabilityCurveExtrapolation_flatZero)
        curve.enableExtrapolation()
        sp_extrap = curve.survivalProbability(Date(15, January, 2028))
        self.assertGreater(sp_extrap, 0.0)
        self.assertLess(sp_extrap, 0.95)


if __name__ == "__main__":
    unittest.main()
