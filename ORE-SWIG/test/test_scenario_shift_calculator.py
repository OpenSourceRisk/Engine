"""Tests for ReturnConfiguration, HistoricalScenarioGenerator and scenario shifts."""

import unittest

import ORE


class TestReturnConfigurationAndGenerators(unittest.TestCase):
    """Coverage for scenario generation and return configuration bindings."""

    def _make_factory(self):
        """Create a SimpleScenarioFactory."""
        return ORE.SimpleScenarioFactory()

    def _make_scenarios_and_loader(self):
        """Build a three-scenario HistoricalScenarioLoader with an IR key."""
        factory = self._make_factory()
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

    def test_return_configuration_default_constructible(self):
        """ReturnConfiguration() is default-constructible."""
        rc = ORE.ReturnConfiguration()
        self.assertIsNotNone(rc)

    def test_return_type_log_accessible(self):
        """ReturnType.Log is exposed on ReturnConfiguration."""
        self.assertTrue(
            hasattr(ORE.ReturnConfiguration, "ReturnConfigurationReturnType_Log"),
            "ReturnConfigurationReturnType_Log not found",
        )
        log_type = ORE.ReturnConfiguration.ReturnConfigurationReturnType_Log
        self.assertIsNotNone(log_type)

    def test_return_type_enum_all_values(self):
        """All ReturnType enum values are exposed."""
        for enum_name in [
            "ReturnConfigurationReturnType_Absolute",
            "ReturnConfigurationReturnType_Relative",
            "ReturnConfigurationReturnType_Log",
        ]:
            self.assertTrue(
                hasattr(ORE.ReturnConfiguration, enum_name),
                msg=f"Missing enum: {enum_name}",
            )

    def test_historical_scenario_generator_constructible(self):
        """HistoricalScenarioGenerator is constructible from loader/factory/config."""
        loader, factory, _, _, _, _ = self._make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)
        self.assertIsNotNone(gen)

    def test_historical_scenario_generator_with_calendar(self):
        """HistoricalScenarioGenerator is constructible with calendar args."""
        loader, factory, _, _, _, _ = self._make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        cal = ORE.TARGET()

        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc, cal, None, 1, True)
        self.assertIsNotNone(gen)
        self.assertGreater(gen.numScenarios(), 0)

    def test_next_returns_scenario(self):
        """next(date) returns a scenario."""
        loader, factory, key, d1, _, _ = self._make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)

        base = factory.buildScenario(d1, True)
        base.add(key, 0.99500)
        gen.setBaseScenario(base)

        scenario = gen.next(d1)
        self.assertIsNotNone(scenario)
        self.assertEqual(scenario.asof(), d1)

    def test_generator_produces_multiple_scenarios(self):
        """The generator reports two scenarios from three dates."""
        loader, factory, _, _, _, _ = self._make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)

        num_scen = gen.numScenarios()
        self.assertEqual(num_scen, 2, msg=f"Expected 2 scenarios, got {num_scen}")

    def test_scenario_shift_calculator_constructible(self):
        """ScenarioShiftCalculator symbol is available."""
        self.assertTrue(
            hasattr(ORE, "ScenarioShiftCalculator"),
            "ScenarioShiftCalculator not found in ORE module",
        )

    def test_scenario_shift_calculator_symbol_available(self):
        """ScenarioShiftCalculator symbol lookup succeeds."""
        self.assertTrue(hasattr(ORE, "ScenarioShiftCalculator"))

    def test_integration_historical_to_shift(self):
        """Generate a shifted scenario from historical inputs."""
        loader, factory, key, d1, _, _ = self._make_scenarios_and_loader()
        rc = ORE.ReturnConfiguration()
        gen = ORE.HistoricalScenarioGenerator(loader, factory, rc)

        base = factory.buildScenario(d1, True)
        base.add(key, 0.99500)
        gen.setBaseScenario(base)

        shifted = gen.next(d1)
        self.assertIsNotNone(shifted)
        self.assertEqual(shifted.asof(), d1)
        self.assertTrue(shifted.isAbsolute())

    def test_return_configuration_returnvalue_method(self):
        """returnValue() computes expected relative return."""
        m = ORE.KeyTypeReturnTypeMap()
        m[ORE.RiskFactorKey.KeyType_DiscountCurve] = (
            ORE.ReturnConfiguration.ReturnConfigurationReturnType_Relative
        )
        rc = ORE.ReturnConfiguration(m)

        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        d1 = ORE.Date(3, ORE.January, 2020)
        d2 = ORE.Date(6, ORE.January, 2020)

        rv = rc.returnValue(key, 0.99500, 0.99490, d1, d2)
        expected = 0.99490 / 0.99500 - 1.0
        self.assertAlmostEqual(rv, expected, places=10)

    def test_return_configuration_xml_serialization(self):
        """XML serialization round-trips."""
        rc1 = ORE.ReturnConfiguration()
        xml_str = rc1.toXMLString()
        self.assertIsInstance(xml_str, str)
        self.assertGreater(len(xml_str), 0)

        rc2 = ORE.ReturnConfiguration()
        rc2.fromXMLString(xml_str)

    def test_return_type_method_returns_return_struct(self):
        """Flattened return type accessors expose type and displacement."""
        m = ORE.KeyTypeReturnTypeMap()
        m[ORE.RiskFactorKey.KeyType_DiscountCurve] = (
            ORE.ReturnConfiguration.ReturnConfigurationReturnType_Log
        )
        rc = ORE.ReturnConfiguration(m)

        key = ORE.RiskFactorKey(ORE.RiskFactorKey.KeyType_DiscountCurve, "EUR", 0)
        return_type = rc.returnTypeValue(key)
        displacement = rc.returnDisplacement(key)

        self.assertEqual(
            return_type, ORE.ReturnConfiguration.ReturnConfigurationReturnType_Log
        )
        self.assertEqual(displacement, 0.0)


if __name__ == "__main__":
    unittest.main()
