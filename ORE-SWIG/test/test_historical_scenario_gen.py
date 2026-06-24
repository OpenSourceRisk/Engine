"""Tests for historical scenario generation SWIG bindings.

Covers: ReturnConfiguration (+ ReturnType enum), HistoricalScenarioGenerator,
        HistoricalScenarioGeneratorRandom, HistoricalScenarioGeneratorTransform,
        SimpleScenarioFactory.
"""

import unittest

import ORE


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_factory():
    return ORE.SimpleScenarioFactory()


def _make_scenarios_and_loader():
    """Build a three-scenario HistoricalScenarioLoader with one EUR discount key."""
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
    return loader, factory, key, d1, d2, d3


# ---------------------------------------------------------------------------
# Symbol availability
# ---------------------------------------------------------------------------

class TestSymbolsAvailable(unittest.TestCase):
    """All phase-1b symbols must be present in the ORE module."""

    EXPECTED = [
        "ReturnConfiguration",
        "KeyTypeReturnTypeMap",
        "HistoricalScenarioGenerator",
        "HistoricalScenarioGeneratorRandom",
        "HistoricalScenarioGeneratorTransform",
        "SimpleScenarioFactory",
    ]

    def test_symbols_present(self):
        for name in self.EXPECTED:
            self.assertTrue(hasattr(ORE, name),
                            msg=f"Missing ORE symbol: {name}")

    def test_return_type_enum_on_class(self):
        """ReturnType enum values are class-level attributes of ReturnConfiguration."""
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Absolute"),
            msg="Missing ReturnConfiguration.ReturnConfigurationReturnType_Absolute")
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Relative"),
            msg="Missing ReturnConfiguration.ReturnConfigurationReturnType_Relative")
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Log"),
            msg="Missing ReturnConfiguration.ReturnConfigurationReturnType_Log")


# ---------------------------------------------------------------------------
# ReturnConfiguration
# ---------------------------------------------------------------------------

class TestReturnConfiguration(unittest.TestCase):

    def test_default_constructor(self):
        rc = ORE.ReturnConfiguration()
        self.assertIsNotNone(rc)

    def test_return_type_enum_values_accessible(self):
        """ReturnType enum values are class-level attributes of ReturnConfiguration."""
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Absolute"))
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Relative"))
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Log"))

    def test_constructor_with_map(self):
        m = ORE.KeyTypeReturnTypeMap()
        m[ORE.RiskFactorKey.KeyType_DiscountCurve] = \
            ORE.ReturnConfiguration.ReturnConfigurationReturnType_Relative
        m[ORE.RiskFactorKey.KeyType_FXSpot] = \
            ORE.ReturnConfiguration.ReturnConfigurationReturnType_Log
        rc = ORE.ReturnConfiguration(m)
        self.assertIsNotNone(rc)

    def test_return_value_relative(self):
        """returnValue with Relative type should compute v2/v1 - 1."""
        m = ORE.KeyTypeReturnTypeMap()
        m[ORE.RiskFactorKey.KeyType_DiscountCurve] = \
            ORE.ReturnConfiguration.ReturnConfigurationReturnType_Relative
        rc = ORE.ReturnConfiguration(m)
        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        d1 = ORE.Date(3, ORE.January, 2020)
        d2 = ORE.Date(6, ORE.January, 2020)
        rv = rc.returnValue(key, 0.99500, 0.99490, d1, d2)
        self.assertAlmostEqual(rv, 0.99490 / 0.99500 - 1.0, places=10)

    def test_xml_round_trip(self):
        """toXMLString/fromXMLString must round-trip without raising."""
        rc = ORE.ReturnConfiguration()
        xml_str = rc.toXMLString()
        self.assertIsInstance(xml_str, str)
        self.assertGreater(len(xml_str), 0)
        rc2 = ORE.ReturnConfiguration()
        rc2.fromXMLString(xml_str)


# ---------------------------------------------------------------------------
# SimpleScenarioFactory
# ---------------------------------------------------------------------------

class TestSimpleScenarioFactory(unittest.TestCase):

    def test_default_constructor(self):
        f = ORE.SimpleScenarioFactory()
        self.assertIsNotNone(f)

    def test_build_scenario(self):
        f = ORE.SimpleScenarioFactory()
        d = ORE.Date(3, ORE.March, 2026)
        s = f.buildScenario(d, True)
        self.assertIsNotNone(s)
        self.assertEqual(s.asof(), d)
        self.assertTrue(s.isAbsolute())

    def test_build_scenario_with_label(self):
        f = ORE.SimpleScenarioFactory()
        d = ORE.Date(3, ORE.March, 2026)
        s = f.buildScenario(d, True, False, "my_label", 1.05)
        self.assertEqual(s.label(), "my_label")


# ---------------------------------------------------------------------------
# HistoricalScenarioGenerator
# ---------------------------------------------------------------------------

class TestHistoricalScenarioGenerator(unittest.TestCase):

    def _make_generator(self):
        loader, factory, key, d1, d2, d3 = _make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)
        return gen, factory, key, d1

    def test_construction(self):
        gen, _, _, _ = self._make_generator()
        self.assertIsNotNone(gen)

    def test_num_scenarios(self):
        gen, _, _, _ = self._make_generator()
        # 3 dates → 2 consecutive pairs
        self.assertEqual(gen.numScenarios(), 2)

    def test_start_end_dates_populated(self):
        gen, _, _, _ = self._make_generator()
        self.assertEqual(len(gen.startDates()), gen.numScenarios())
        self.assertEqual(len(gen.endDates()),   gen.numScenarios())

    def test_next_returns_scenario(self):
        gen, factory, key, d1 = self._make_generator()
        base = factory.buildScenario(d1, True)
        base.add(key, 0.99500)
        gen.setBaseScenario(base)
        s = gen.next(d1)
        self.assertIsNotNone(s)

    def test_reset_allows_replay(self):
        gen, factory, key, d1 = self._make_generator()
        base = factory.buildScenario(d1, True)
        base.add(key, 0.99500)
        gen.setBaseScenario(base)
        gen.next(d1)
        gen.reset()
        s2 = gen.next(d1)
        self.assertIsNotNone(s2)

    def test_accessor_methods(self):
        gen, _, _, _ = self._make_generator()
        self.assertIsNotNone(gen.scenarioLoader())
        self.assertIsNotNone(gen.scenarioFactory())
        self.assertEqual(gen.labelPrefix(), "")
        self.assertFalse(gen.generateDifferenceScenarios())

    def test_set_generate_difference_scenarios(self):
        gen, _, _, _ = self._make_generator()
        gen.setGenerateDifferenceScenarios(True)
        self.assertTrue(gen.generateDifferenceScenarios())

    def test_construction_with_calendar(self):
        loader, factory, key, d1, d2, d3 = _make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        cal = ORE.TARGET()
        # adjFactors=None, mporDays=1 so consecutive-day shifts fit 3 nearby dates
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc, cal,
                                             None, 1, True)
        self.assertIsNotNone(gen)
        self.assertGreater(gen.numScenarios(), 0)


# ---------------------------------------------------------------------------
# HistoricalScenarioGeneratorRandom
# ---------------------------------------------------------------------------

class TestHistoricalScenarioGeneratorRandom(unittest.TestCase):

    def test_construction(self):
        loader, factory, key, d1, d2, d3 = _make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        cal = ORE.TARGET()
        gen = ORE.HistoricalScenarioGeneratorRandom(loader, factory, rc, cal)
        self.assertIsNotNone(gen)

    def test_inherits_num_scenarios(self):
        loader, factory, key, d1, d2, d3 = _make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        cal = ORE.TARGET()
        # mporDays=1 so consecutive-day shifts fit the 3 nearby dates
        gen = ORE.HistoricalScenarioGeneratorRandom(loader, factory, rc, cal,
                                                    None, 1, True)
        self.assertGreater(gen.numScenarios(), 0)

    def test_reset(self):
        loader, factory, key, d1, d2, d3 = _make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        cal = ORE.TARGET()
        gen = ORE.HistoricalScenarioGeneratorRandom(loader, factory, rc, cal)
        gen.reset()
        self.assertIsNotNone(gen)


# ---------------------------------------------------------------------------
# if __name__ == "__main__"
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    unittest.main()

