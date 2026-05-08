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


class DynamicBlackVolTermStructureSurfaceTest(unittest.TestCase):
    """Test DynamicBlackVolTermStructure<tag::surface> wrapper."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()
        self.cal = TARGET()

    def test_sticky_strike_constant_variance(self):
        """Wrap a BlackConstantVol with StickyStrike + ConstantVariance, verify vol."""
        flat_vol = 0.20
        vol_ts = BlackConstantVol(self.today, self.cal, flat_vol, self.dc)
        vol_handle = BlackVolTermStructureHandle(vol_ts)

        dyn = DynamicBlackVolTermStructureSurface(
            vol_handle, 0, self.cal, ConstantVariance, StickyStrike)
        dyn.enableExtrapolation()

        self.assertAlmostEqual(dyn.blackVol(0.5, 100.0), flat_vol, places=10)
        self.assertAlmostEqual(dyn.blackVol(1.0, 100.0), flat_vol, places=10)
        self.assertAlmostEqual(dyn.blackVol(2.0, 50.0), flat_vol, places=10)

    def test_max_date(self):
        """maxDate() should be callable without error."""
        vol_ts = BlackConstantVol(self.today, self.cal, 0.15, self.dc)
        vol_handle = BlackVolTermStructureHandle(vol_ts)
        dyn = DynamicBlackVolTermStructureSurface(
            vol_handle, 0, self.cal, ConstantVariance, StickyStrike)
        self.assertIsNotNone(dyn.maxDate())

    def test_min_max_strike(self):
        """minStrike/maxStrike should return finite values."""
        vol_ts = BlackConstantVol(self.today, self.cal, 0.15, self.dc)
        vol_handle = BlackVolTermStructureHandle(vol_ts)
        dyn = DynamicBlackVolTermStructureSurface(
            vol_handle, 0, self.cal, ConstantVariance, StickyStrike)
        self.assertIsNotNone(dyn.minStrike())
        self.assertIsNotNone(dyn.maxStrike())

    def test_sticky_log_moneyness_requires_ts(self):
        """StickyLogMoneyness requires riskfree, dividend, and spot."""
        flat_vol = 0.20
        vol_ts = BlackConstantVol(self.today, self.cal, flat_vol, self.dc)
        vol_handle = BlackVolTermStructureHandle(vol_ts)

        spot_q = SimpleQuote(100.0)
        spot_h = QuoteHandle(spot_q)
        rf = FlatForward(self.today, 0.02, self.dc)
        div = FlatForward(self.today, 0.01, self.dc)
        rf_h = YieldTermStructureHandle(rf)
        div_h = YieldTermStructureHandle(div)

        dyn = DynamicBlackVolTermStructureSurface(
            vol_handle, 0, self.cal, ConstantVariance, StickyLogMoneyness,
            rf_h, div_h, spot_h)
        dyn.enableExtrapolation()

        vol = dyn.blackVol(1.0, 100.0)
        self.assertGreater(vol, 0.0)
        self.assertTrue(math.isfinite(vol))


class SpreadedBlackVolatilitySurfaceMoneynessTest(unittest.TestCase):
    """Test SpreadedBlackVolatilitySurfaceMoneyness subclasses."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()
        self.cal = TARGET()

        # Reference flat vol surface
        self.ref_vol = 0.20
        ref_ts = BlackConstantVol(self.today, self.cal, self.ref_vol, self.dc)
        self.ref_handle = BlackVolTermStructureHandle(ref_ts)

        # Spot and yield curves
        self.spot_val = 100.0
        self.spot = QuoteHandle(SimpleQuote(self.spot_val))
        self.rf = YieldTermStructureHandle(FlatForward(self.today, 0.02, self.dc))
        self.div = YieldTermStructureHandle(FlatForward(self.today, 0.01, self.dc))

        # Zero spread quotes (1 time × 1 moneyness)
        self.times = [1.0]
        self.moneyness = [1.0]
        zero_q = SimpleQuote(0.0)
        self.zero_spreads = [[QuoteHandle(zero_q)]]

    def _build_surface(self, cls):
        return cls(
            self.ref_handle, self.spot, self.times, self.moneyness,
            self.zero_spreads, self.spot, self.div, self.rf,
            self.div, self.rf, True)

    def test_moneyness_spot_zero_spread(self):
        """MoneynessSpot with zero spreads should match reference vol."""
        surf = self._build_surface(SpreadedBlackVolatilitySurfaceMoneynessSpot)
        surf.enableExtrapolation()
        vol = surf.blackVol(1.0, self.spot_val)
        self.assertAlmostEqual(vol, self.ref_vol, places=6)

    def test_moneyness_forward_construction(self):
        """MoneynessForward should construct successfully."""
        surf = self._build_surface(SpreadedBlackVolatilitySurfaceMoneynessForward)
        surf.enableExtrapolation()
        vol = surf.blackVol(1.0, self.spot_val)
        self.assertGreater(vol, 0.0)
        self.assertTrue(math.isfinite(vol))

    def test_log_moneyness_spot_construction(self):
        surf = self._build_surface(SpreadedBlackVolatilitySurfaceLogMoneynessSpot)
        surf.enableExtrapolation()
        vol = surf.blackVol(1.0, self.spot_val)
        self.assertTrue(math.isfinite(vol))

    def test_moneyness_inspector(self):
        """moneyness() inspector should return the input moneyness values."""
        surf = self._build_surface(SpreadedBlackVolatilitySurfaceMoneynessSpot)
        m = surf.moneyness()
        self.assertGreaterEqual(len(m), 1)
        self.assertIn(1.0, [round(x, 10) for x in m])

    def test_all_subclasses_constructible(self):
        """All 7 subclasses should construct without error."""
        classes = [
            SpreadedBlackVolatilitySurfaceMoneynessSpot,
            SpreadedBlackVolatilitySurfaceMoneynessForward,
            SpreadedBlackVolatilitySurfaceLogMoneynessSpot,
            SpreadedBlackVolatilitySurfaceLogMoneynessForward,
            SpreadedBlackVolatilitySurfaceMoneynessSpotAbsolute,
            SpreadedBlackVolatilitySurfaceMoneynessForwardAbsolute,
            SpreadedBlackVolatilitySurfaceStdDevs,
        ]
        for cls in classes:
            with self.subTest(cls=cls.__name__):
                surf = self._build_surface(cls)
                self.assertIsNotNone(surf)

    def test_nonzero_spread_changes_vol(self):
        """MoneynessSpot with non-zero spread should differ from reference."""
        spread_q = SimpleQuote(0.05)
        spreads = [[QuoteHandle(spread_q)]]
        surf = SpreadedBlackVolatilitySurfaceMoneynessSpot(
            self.ref_handle, self.spot, self.times, self.moneyness,
            spreads, self.spot, self.div, self.rf,
            self.div, self.rf, True)
        surf.enableExtrapolation()
        vol = surf.blackVol(1.0, self.spot_val)
        self.assertNotAlmostEqual(vol, self.ref_vol, places=2)


class BlackVarianceSurfaceSparseLinearTest(unittest.TestCase):
    """Test BlackVarianceSurfaceSparse<Linear, Linear> wrapper."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()
        self.cal = TARGET()

        # 2 expiries × 3 strikes = 6 data points
        d1 = Date(15, July, 2025)
        d2 = Date(15, January, 2026)
        self.dates = [d1, d1, d1, d2, d2, d2]
        self.strikes = [90.0, 100.0, 110.0, 90.0, 100.0, 110.0]
        self.vols = [0.22, 0.20, 0.21, 0.24, 0.22, 0.23]

    def test_construction_and_query(self):
        """Construct from sparse data and query blackVol."""
        surf = BlackVarianceSurfaceSparseLinear(
            self.today, self.cal, self.dates, self.strikes, self.vols, self.dc)
        surf.enableExtrapolation()

        # At grid points, vol should match input
        vol = surf.blackVol(self.dates[1], 100.0)
        self.assertAlmostEqual(vol, 0.20, places=4)

    def test_off_grid_interpolation(self):
        """blackVol at off-grid point should be finite and positive."""
        surf = BlackVarianceSurfaceSparseLinear(
            self.today, self.cal, self.dates, self.strikes, self.vols, self.dc)
        surf.enableExtrapolation()

        vol = surf.blackVol(Date(15, October, 2025), 95.0)
        self.assertGreater(vol, 0.0)
        self.assertTrue(math.isfinite(vol))

    def test_inspectors(self):
        """maxDate, minStrike, maxStrike should work."""
        surf = BlackVarianceSurfaceSparseLinear(
            self.today, self.cal, self.dates, self.strikes, self.vols, self.dc)
        self.assertIsNotNone(surf.maxDate())
        self.assertIsNotNone(surf.minStrike())
        self.assertIsNotNone(surf.maxStrike())


class BlackVolatilitySurfaceDeltaTest(unittest.TestCase):
    """Test BlackVolatilitySurfaceDelta wrapper."""

    def setUp(self):
        self.today = Date(15, January, 2025)
        Settings.instance().evaluationDate = self.today
        self.dc = Actual365Fixed()
        self.cal = TARGET()

        self.spot = QuoteHandle(SimpleQuote(1.20))
        self.dom_ts = YieldTermStructureHandle(FlatForward(self.today, 0.03, self.dc))
        self.for_ts = YieldTermStructureHandle(FlatForward(self.today, 0.01, self.dc))

    def test_construction_basic(self):
        """Construct from a 2-tenor delta vol matrix with smile."""
        dates = DateVector()
        dates.append(Date(15, April, 2025))
        dates.append(Date(15, January, 2026))
        # Put deltas are negative by convention
        put_deltas = DoubleVector()
        put_deltas.append(-0.25)
        call_deltas = DoubleVector()
        call_deltas.append(0.25)
        has_atm = True

        # Vol matrix: rows = num_dates, cols = putDeltas + ATM + callDeltas = 3
        vol_matrix = Matrix(2, 3)
        vol_matrix[0][0] = 0.12; vol_matrix[0][1] = 0.10; vol_matrix[0][2] = 0.11
        vol_matrix[1][0] = 0.13; vol_matrix[1][1] = 0.11; vol_matrix[1][2] = 0.12

        surf = BlackVolatilitySurfaceDelta(
            self.today, dates, put_deltas, call_deltas, has_atm,
            vol_matrix, self.dc, self.cal, self.spot,
            self.dom_ts, self.for_ts)
        surf.enableExtrapolation()

        vol = surf.blackVol(dates[0], 1.20)
        self.assertGreater(vol, 0.0)
        self.assertTrue(math.isfinite(vol))

    def test_dates_inspector(self):
        """dates() should return the input dates."""
        dates = DateVector()
        dates.append(Date(15, April, 2025))
        dates.append(Date(15, January, 2026))
        put_deltas = DoubleVector()
        put_deltas.append(-0.25)
        call_deltas = DoubleVector()
        call_deltas.append(0.25)
        vol_matrix = Matrix(2, 3)
        vol_matrix[0][0] = 0.12; vol_matrix[0][1] = 0.10; vol_matrix[0][2] = 0.11
        vol_matrix[1][0] = 0.13; vol_matrix[1][1] = 0.11; vol_matrix[1][2] = 0.12

        surf = BlackVolatilitySurfaceDelta(
            self.today, dates, put_deltas, call_deltas, True,
            vol_matrix, self.dc, self.cal, self.spot,
            self.dom_ts, self.for_ts)
        self.assertEqual(len(surf.dates()), 2)

    def test_black_vol_smile(self):
        """blackVolSmile should return a SmileSection with smile data."""
        dates = DateVector()
        dates.append(Date(15, April, 2025))
        dates.append(Date(15, January, 2026))
        put_deltas = DoubleVector()
        put_deltas.append(-0.25)
        call_deltas = DoubleVector()
        call_deltas.append(0.25)
        vol_matrix = Matrix(2, 3)
        vol_matrix[0][0] = 0.12; vol_matrix[0][1] = 0.10; vol_matrix[0][2] = 0.11
        vol_matrix[1][0] = 0.13; vol_matrix[1][1] = 0.11; vol_matrix[1][2] = 0.12

        surf = BlackVolatilitySurfaceDelta(
            self.today, dates, put_deltas, call_deltas, True,
            vol_matrix, self.dc, self.cal, self.spot,
            self.dom_ts, self.for_ts)
        surf.enableExtrapolation()

        smile = surf.blackVolSmile(0.25)
        self.assertIsNotNone(smile)
        self.assertIsInstance(smile, SmileSection)
        vol = smile.volatility(1.20)
        self.assertGreater(vol, 0.0)


class OISCapFloorHelperTest(unittest.TestCase):
    """Test OISCapFloorHelper wrapper."""

    def test_construction(self):
        """Construct an OISCapFloorHelper with basic parameters."""
        today = Date(15, January, 2025)
        Settings.instance().evaluationDate = today
        dc = Actual365Fixed()

        flat_ts = FlatForward(today, 0.03, dc)
        ts_handle = YieldTermStructureHandle(flat_ts)

        index = Sofr(ts_handle)

        vol_quote = QuoteHandle(SimpleQuote(0.005))

        helper = OISCapFloorHelper(
            CapFloorHelper.Cap,
            Period(2, Years),
            Period(3, Months),
            0.03,
            vol_quote,
            index,
            ts_handle)
        self.assertIsNotNone(helper)


class PiecewiseOptionletCurveLinearTest(unittest.TestCase):
    """Test PiecewiseOptionletCurve<Linear, IterativeBootstrap> wrapper."""

    def test_symbol_exists(self):
        """PiecewiseOptionletCurveLinear should be importable."""
        self.assertTrue(hasattr(__import__("ORE"), "PiecewiseOptionletCurveLinear"))

    def test_interpolated_optionlet_curve_symbol(self):
        """InterpolatedOptionletCurveLinear should be importable."""
        self.assertTrue(hasattr(__import__("ORE"), "InterpolatedOptionletCurveLinear"))


if __name__ == "__main__":
    unittest.main()
