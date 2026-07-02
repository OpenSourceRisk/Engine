"""Tests for Ticket 4: PNLCalculator, CovarianceCalculator, HistoricalSensiPnlCalculator.

Covers the acceptance criteria:
- PNLCalculator constructible; pnls() returns a Python list of floats after populatePNLs()
- HistoricalSensiPnlCalculator constructible from wrapped generator + stream
- calculateSensiPnl() populates the PNL calculator with per-scenario PnL
- CovarianceCalculator.covariance() returns a QuantLib Matrix accessible from Python
- Integration test: sensitivity records + historical scenarios -> calculateSensiPnl()
  -> verify PnL vector length matches scenario count
"""

import unittest

import ORE


def _make_time_period(dates):
    return ORE.TimePeriod(dates)


def _make_factory():
    return ORE.SimpleScenarioFactory()


def _make_generator():
    """Build a three-date historical scenario generator (-> 2 scenarios)."""
    factory = _make_factory()
    key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)

    d1 = ORE.Date(3, ORE.January, 2020)
    d2 = ORE.Date(6, ORE.January, 2020)
    d3 = ORE.Date(7, ORE.January, 2020)

    s1 = factory.buildScenario(d1, True)
    s2 = factory.buildScenario(d2, True)
    s3 = factory.buildScenario(d3, True)
    for s, v in [(s1, 0.99500), (s2, 0.99490), (s3, 0.99480)]:
        s.add(key, v)

    vec = ORE.ScenarioVector()
    for s in (s1, s2, s3):
        vec.append(s)

    date_set = ORE.DateSet()
    for d in (d1, d2, d3):
        date_set.insert(d)

    loader = ORE.HistoricalScenarioLoader(vec, date_set)
    rc = ORE.ReturnConfiguration()
    gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)
    return gen, key, d1, d2, d3


class TestPNLCalculator(unittest.TestCase):
    """AC1: PNLCalculator constructible; pnls()/foPnls() populated."""

    def test_constructible(self):
        tp = _make_time_period([ORE.Date(3, ORE.January, 2020), ORE.Date(7, ORE.January, 2020)])
        calc = ORE.PNLCalculator(tp)
        self.assertIsNotNone(calc)

    def test_populate_and_read_pnls(self):
        d1 = ORE.Date(3, ORE.January, 2020)
        d2 = ORE.Date(6, ORE.January, 2020)
        d3 = ORE.Date(7, ORE.January, 2020)
        tp = _make_time_period([d1, d3])
        calc = ORE.PNLCalculator(tp)

        all_pnls = [1.5, -2.5]
        fo_pnls = [1.0, -2.0]
        start_dates = [d1, d2]
        end_dates = [d2, d3]
        calc.populatePNLs(all_pnls, fo_pnls, start_dates, end_dates)

        self.assertEqual(list(calc.pnls()), all_pnls)
        self.assertEqual(list(calc.foPnls()), fo_pnls)

    def test_populate_trade_pnls(self):
        tp = _make_time_period([ORE.Date(3, ORE.January, 2020), ORE.Date(7, ORE.January, 2020)])
        calc = ORE.PNLCalculator(tp)

        trade_pnls = ORE.DoubleVectorVector()
        trade_pnls.append([1.0, 2.0])
        fo_trade_pnls = ORE.DoubleVectorVector()
        fo_trade_pnls.append([0.5, 1.5])

        calc.populateTradePNLs(trade_pnls, fo_trade_pnls)
        self.assertEqual(len(calc.tradePnls()), 1)
        self.assertEqual(list(calc.tradePnls()[0]), [1.0, 2.0])

    def test_clear(self):
        tp = _make_time_period([ORE.Date(3, ORE.January, 2020), ORE.Date(7, ORE.January, 2020)])
        calc = ORE.PNLCalculator(tp)
        calc.populatePNLs([1.0], [1.0], [ORE.Date(3, ORE.January, 2020)], [ORE.Date(6, ORE.January, 2020)])
        calc.clear()
        self.assertEqual(len(calc.pnls()), 0)


class TestCovarianceCalculator(unittest.TestCase):
    """AC3: CovarianceCalculator.covariance()/correlation() return a QuantLib Matrix."""

    def test_constructible(self):
        tp = _make_time_period([ORE.Date(3, ORE.January, 2020), ORE.Date(7, ORE.January, 2020)])
        calc = ORE.CovarianceCalculator(tp)
        self.assertIsNotNone(calc)

    def test_initialise_and_populate_covariance(self):
        tp = _make_time_period([ORE.Date(3, ORE.January, 2020), ORE.Date(7, ORE.January, 2020)])
        calc = ORE.CovarianceCalculator(tp)

        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        keys = ORE.RiskFactorKeySizePairSet()
        keys.insert(key, 0)

        calc.initialise(keys)
        calc.populateCovariance(keys)

        matrix = calc.covariance()
        self.assertEqual(matrix.rows(), 1)
        self.assertEqual(matrix.columns(), 1)

        corr = calc.correlation()
        self.assertEqual(corr.rows(), 1)


class TestHistoricalSensiPnlCalculator(unittest.TestCase):
    """AC2/AC4/AC5: HistoricalSensiPnlCalculator construction and calculateSensiPnl()."""

    def test_constructible(self):
        gen, _, _, _, _ = _make_generator()
        stream = ORE.SensitivityInMemoryStream()
        calc = ORE.HistoricalSensiPnlCalculator(gen, stream)
        self.assertIsNotNone(calc)

    def test_get_scenario_number(self):
        gen, _, _, _, _ = _make_generator()
        stream = ORE.SensitivityInMemoryStream()
        calc = ORE.HistoricalSensiPnlCalculator(gen, stream)
        self.assertEqual(calc.getScenarioNumber(), gen.numScenarios())

    def test_calculate_sensi_pnl_populates_pnl_vector(self):
        """AC5: build a shift cube + sensitivity record set manually and verify
        that calculateSensiPnl() populates the PNL calculator with a PnL vector
        whose length matches the number of historical scenarios."""
        gen, key, d1, d2, d3 = _make_generator()
        base = ORE.SimpleScenarioFactory().buildScenario(d1, True)
        base.add(key, 0.99500)
        gen.setBaseScenario(base)

        stream = ORE.SensitivityInMemoryStream()
        record = ORE.SensitivityRecord(
            "trade1", False, key, "EUR discount", 0.0001,
            ORE.RiskFactorKey(), "", 0.0, "EUR", 100.0, 5000.0, 0.0)
        stream.add(record)

        sensi_calc = ORE.HistoricalSensiPnlCalculator(gen, stream)
        num_scenarios = gen.numScenarios()

        # Manually build the shift cube: one risk factor id, one date, num_scenarios samples.
        key_id = "DiscountCurve/EUR/0"
        ids = ORE.StringSet()
        ids.insert(key_id)
        cube = ORE.DoublePrecisionInMemoryCubeN(d1, ids, [d1], num_scenarios)
        for i in range(num_scenarios):
            cube.set(0.0001 * (i + 1), 0, 0, i, 0)

        rf_keys = ORE.RiskFactorKeyVector()
        rf_keys.append(key)

        srs = ORE.SensitivityRecordSet()
        srs.insert(record)

        tp = _make_time_period([d1, d3])
        pnl_calc = ORE.PNLCalculator(tp)
        pnl_calculators = ORE.PNLCalculatorVector()
        pnl_calculators.append(pnl_calc)

        sensi_calc.calculateSensiPnl(srs, rf_keys, cube, pnl_calculators, None)

        self.assertEqual(len(pnl_calc.pnls()), num_scenarios)
        self.assertEqual(len(pnl_calc.foPnls()), num_scenarios)


if __name__ == "__main__":
    unittest.main()
