"""Smoke tests for QuantExt model bindings."""

from ORE import *
import unittest


class QuantExtModelBindingSmokeTest(unittest.TestCase):
    def test_model_symbols_available(self):
        """Ensure key model classes are exposed in the module."""
        self.assertTrue(hasattr(__import__("ORE"), "LinkableCalibratedModel"))
        self.assertTrue(hasattr(__import__("ORE"), "Parametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "IrModel"))
        self.assertTrue(hasattr(__import__("ORE"), "LinearGaussMarkovModel"))
        self.assertTrue(hasattr(__import__("ORE"), "HwModel"))
        self.assertTrue(hasattr(__import__("ORE"), "CrossAssetModel"))
        self.assertTrue(hasattr(__import__("ORE"), "HullWhiteBucketing"))

    def test_concrete_parametrization_symbols_available(self):
        """Ensure concrete parametrization classes are exposed."""
        self.assertTrue(hasattr(__import__("ORE"), "IrLgm1fConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "IrLgm1fPiecewiseConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "FxBsConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "FxBsPiecewiseConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "EqBsConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "IrHwConstantParametrization"))
        self.assertTrue(hasattr(__import__("ORE"), "FxEqOptionHelper"))

    def test_hull_white_bucketing_constructs(self):
        """Build a HullWhiteBucketing instance and inspect basic state."""
        hwb = HullWhiteBucketing(-2.0, 2.0, 8)
        self.assertGreaterEqual(hwb.buckets(), 2)

    def test_transition_matrix_utilities_are_callable(self):
        """Call transition matrix utility functions from bindings."""
        matrix = Matrix(2, 2, 0.0)
        matrix[0][0] = 0.9
        matrix[0][1] = 0.1
        matrix[1][0] = 0.2
        matrix[1][1] = 0.8

        sanitiseTransitionMatrix(matrix)
        checkTransitionMatrix(matrix)
        try:
            generator_matrix = generator(matrix, 1.0)
            self.assertEqual(generator_matrix.rows(), 2)
            self.assertEqual(generator_matrix.columns(), 2)
        except RuntimeError as error:
            self.assertIn("Logm()", str(error))


class QuantExtModelConstructionTest(unittest.TestCase):
    """Tests for direct Python construction of QuantExt models."""

    def _make_flat_yts(self, rate=0.03):
        """Create a flat yield term structure handle for testing."""
        today = Date(15, January, 2025)
        Settings.instance().evaluationDate = today
        return YieldTermStructureHandle(
            FlatForward(today, QuoteHandle(SimpleQuote(rate)), Actual365Fixed()))

    def test_lgm_constant_parametrization(self):
        """Build IrLgm1fConstantParametrization and verify values."""
        yts = self._make_flat_yts()
        param = IrLgm1fConstantParametrization(USDCurrency(), yts, 0.01, 0.05)
        self.assertAlmostEqual(param.alpha(1.0), 0.01, places=10)
        self.assertAlmostEqual(param.kappa(1.0), 0.05, places=10)
        self.assertGreater(param.H(1.0), 0.0)
        self.assertGreater(param.zeta(1.0), 0.0)

    def test_lgm_piecewise_parametrization(self):
        """Build IrLgm1fPiecewiseConstantParametrization."""
        yts = self._make_flat_yts()
        alpha_times = Array(2)
        alpha_times[0] = 1.0
        alpha_times[1] = 2.0
        alpha_vals = Array(3)
        alpha_vals[0] = 0.01
        alpha_vals[1] = 0.02
        alpha_vals[2] = 0.015
        kappa_times = Array(1)
        kappa_times[0] = 1.5
        kappa_vals = Array(2)
        kappa_vals[0] = 0.05
        kappa_vals[1] = 0.04
        param = IrLgm1fPiecewiseConstantParametrization(
            USDCurrency(), yts, alpha_times, alpha_vals, kappa_times, kappa_vals)
        self.assertGreater(param.alpha(0.5), 0.0)
        self.assertGreater(param.H(1.0), 0.0)

    def test_lgm_model_from_constant_param(self):
        """Construct LinearGaussMarkovModel from constant parametrization."""
        yts = self._make_flat_yts()
        param = IrLgm1fConstantParametrization(USDCurrency(), yts, 0.01, 0.05)
        model = LinearGaussMarkovModel(param)
        self.assertIsNotNone(model.parametrization())
        self.assertIsNotNone(model.stateProcess())
        db = model.discountBond(0.0, 1.0, 0.0)
        self.assertGreater(db, 0.0)
        self.assertLess(db, 1.5)

    def test_fx_bs_constant_parametrization(self):
        """Build FxBsConstantParametrization."""
        fx_spot = QuoteHandle(SimpleQuote(1.10))
        param = FxBsConstantParametrization(EURCurrency(), fx_spot, 0.10)
        self.assertAlmostEqual(param.sigma(1.0), 0.10, places=10)
        self.assertAlmostEqual(param.variance(1.0), 0.01, places=10)

    def test_fx_bs_piecewise_parametrization(self):
        """Build FxBsPiecewiseConstantParametrization."""
        fx_spot = QuoteHandle(SimpleQuote(1.10))
        times = Array(1)
        times[0] = 1.0
        sigmas = Array(2)
        sigmas[0] = 0.10
        sigmas[1] = 0.12
        param = FxBsPiecewiseConstantParametrization(EURCurrency(), fx_spot, times, sigmas)
        self.assertGreater(param.sigma(0.5), 0.0)

    def test_eq_bs_constant_parametrization(self):
        """Build EqBsConstantParametrization with eqName argument."""
        yts = self._make_flat_yts(0.03)
        div_yts = self._make_flat_yts(0.01)
        eq_spot = QuoteHandle(SimpleQuote(100.0))
        fx_spot = QuoteHandle(SimpleQuote(1.0))
        param = EqBsConstantParametrization(
            USDCurrency(), "SPX", eq_spot, fx_spot, 0.20, yts, div_yts)
        self.assertAlmostEqual(param.sigma(1.0), 0.20, places=10)

    def test_hw_constant_parametrization(self):
        """Build IrHwConstantParametrization (1-factor)."""
        yts = self._make_flat_yts()
        sigma = Matrix(1, 1, 0.01)
        kappa = Array(1)
        kappa[0] = 0.05
        param = IrHwConstantParametrization(USDCurrency(), yts, sigma, kappa)
        self.assertEqual(param.n(), 1)
        self.assertEqual(param.m(), 1)

    def test_hw_model_from_constant_param(self):
        """Construct HwModel from IrHwConstantParametrization."""
        yts = self._make_flat_yts()
        sigma = Matrix(1, 1, 0.01)
        kappa = Array(1)
        kappa[0] = 0.05
        param = IrHwConstantParametrization(USDCurrency(), yts, sigma, kappa)
        model = HwModel(param)
        self.assertIsNotNone(model.parametrization())
        self.assertIsNotNone(model.stateProcess())

    def test_cross_asset_model_ir_fx_constructor(self):
        """Assemble CrossAssetModel from IR models and FX parametrizations."""
        yts_usd = self._make_flat_yts(0.03)
        yts_eur = self._make_flat_yts(0.02)

        lgm_usd = IrLgm1fConstantParametrization(USDCurrency(), yts_usd, 0.01, 0.05)
        lgm_eur = IrLgm1fConstantParametrization(EURCurrency(), yts_eur, 0.015, 0.04)

        model_usd = LinearGaussMarkovModel(lgm_usd)
        model_eur = LinearGaussMarkovModel(lgm_eur)

        fx_spot = QuoteHandle(SimpleQuote(1.10))
        fx_param = FxBsConstantParametrization(EURCurrency(), fx_spot, 0.10)

        ir_models = IrModelVector()
        ir_models.push_back(toIrModel(model_usd))
        ir_models.push_back(toIrModel(model_eur))

        fx_params = FxBsParametrizationVector()
        fx_params.push_back(toFxBsParametrization(fx_param))

        cam = CrossAssetModel(ir_models, fx_params)
        self.assertEqual(cam.components(CrossAssetModel.AssetType_IR), 2)
        self.assertEqual(cam.components(CrossAssetModel.AssetType_FX), 1)
        self.assertGreater(cam.dimension(), 0)

    def test_cross_asset_model_parametrization_vector_constructor(self):
        """Assemble CrossAssetModel from parametrization vector."""
        yts_usd = self._make_flat_yts(0.03)
        yts_eur = self._make_flat_yts(0.02)

        lgm_usd = IrLgm1fConstantParametrization(USDCurrency(), yts_usd, 0.01, 0.05)
        lgm_eur = IrLgm1fConstantParametrization(EURCurrency(), yts_eur, 0.015, 0.04)

        fx_spot = QuoteHandle(SimpleQuote(1.10))
        fx_param = FxBsConstantParametrization(EURCurrency(), fx_spot, 0.10)

        params = ParametrizationVector()
        params.push_back(toParametrization(lgm_usd))
        params.push_back(toParametrization(lgm_eur))
        params.push_back(toParametrization(fx_param))

        cam = CrossAssetModel(params)
        self.assertEqual(cam.components(CrossAssetModel.AssetType_IR), 2)
        self.assertEqual(cam.components(CrossAssetModel.AssetType_FX), 1)

    def test_fx_eq_option_helper_period_constructor(self):
        """Construct FxEqOptionHelper using period-based maturity."""
        yts_dom = self._make_flat_yts(0.03)
        yts_for = self._make_flat_yts(0.02)
        spot = QuoteHandle(SimpleQuote(1.10))
        vol = QuoteHandle(SimpleQuote(0.10))
        helper = FxEqOptionHelper(
            Period(1, Years), TARGET(), 1.10, spot, vol, yts_dom, yts_for)
        self.assertIsNotNone(helper)

    def test_fx_eq_option_helper_date_constructor(self):
        """Construct FxEqOptionHelper using explicit exercise date."""
        yts_dom = self._make_flat_yts(0.03)
        yts_for = self._make_flat_yts(0.02)
        spot = QuoteHandle(SimpleQuote(1.10))
        vol = QuoteHandle(SimpleQuote(0.10))
        exercise_date = Date(15, January, 2026)
        helper = FxEqOptionHelper(
            exercise_date, 1.10, spot, vol, yts_dom, yts_for)
        self.assertIsNotNone(helper)

    def test_vector_upcast_parametrization(self):
        """Verify shared_ptr upcast into ParametrizationVector via push_back."""
        yts = self._make_flat_yts()
        lgm = IrLgm1fConstantParametrization(USDCurrency(), yts, 0.01, 0.05)
        fx = FxBsConstantParametrization(
            EURCurrency(), QuoteHandle(SimpleQuote(1.10)), 0.10)
        vec = ParametrizationVector()
        vec.push_back(toParametrization(lgm))
        vec.push_back(toParametrization(fx))
        self.assertEqual(len(vec), 2)

    def test_vector_upcast_ir_model(self):
        """Verify shared_ptr upcast into IrModelVector via push_back."""
        yts = self._make_flat_yts()
        param = IrLgm1fConstantParametrization(USDCurrency(), yts, 0.01, 0.05)
        model = LinearGaussMarkovModel(param)
        vec = IrModelVector()
        vec.push_back(toIrModel(model))
        self.assertEqual(len(vec), 1)

    def test_vector_upcast_fx_bs_parametrization(self):
        """Verify shared_ptr upcast into FxBsParametrizationVector via push_back."""
        fx = FxBsConstantParametrization(
            EURCurrency(), QuoteHandle(SimpleQuote(1.10)), 0.10)
        vec = FxBsParametrizationVector()
        vec.push_back(toFxBsParametrization(fx))
        self.assertEqual(len(vec), 1)


if __name__ == "__main__":
    unittest.main()
