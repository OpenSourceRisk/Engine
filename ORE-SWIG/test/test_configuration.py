"""Targeted binding and XML round-trip tests for configuration types."""

import unittest

import ORE as ore


class ConfigurationBindingsTest(unittest.TestCase):
    """Validate newly wrapped configuration-side OREData bindings."""

    def _assert_roundtrip(self, instance, factory, expected_fragments=None):
        """Round-trip an XML-serializable object and return the clone."""
        xml = instance.toXMLString()
        self.assertTrue(xml)

        clone = factory()
        clone.fromXMLString(xml)

        roundtripped = clone.toXMLString()
        self.assertTrue(roundtripped)

        for fragment in expected_fragments or []:
            self.assertIn(fragment, roundtripped)

        return clone

    def test_bootstrap_config_roundtrip(self):
        cfg = ore.BootstrapConfig(
            1.0e-10, 1.0e-10, False, 4, 2.5, 1.5, 8, False, 0.0
        )
        self._assert_roundtrip(cfg, ore.BootstrapConfig)

    def test_security_config_api_exposed(self):
        self.assertTrue(hasattr(ore.SecurityConfig, "fromXML"))
        self.assertTrue(hasattr(ore.SecurityConfig, "toXML"))

    def test_configuration_gap_types_are_exposed(self):
        self.assertTrue(hasattr(ore.PriceSegment, "withOffPeakDaily"))
        self.assertTrue(hasattr(ore.ParametricSmileConfiguration, "fromData"))
        self.assertTrue(hasattr(ore.DefaultCurveConfig, "fromSingleConfig"))
        self.assertTrue(hasattr(ore.BaselTrafficLightData, "setObservationData"))

    def test_parametric_smile_configuration_roundtrip(self):
        initial_values = ore.DoubleVectorVector()
        initial_values.push_back([0.15])

        config = ore.ParametricSmileConfiguration.fromData(
            ["alpha"], initial_values, ["Calibrated"], 7, 0.001, 0.01
        )
        clone_initial_values = ore.DoubleVectorVector()
        clone_initial_values.push_back([0.0])
        config_copy = self._assert_roundtrip(
            config,
            lambda: ore.ParametricSmileConfiguration.fromData(
                ["alpha"], clone_initial_values, ["Fixed"], 1, 1.0, 1.0
            ),
            ["alpha", "Calibrated"],
        )

        self.assertEqual(config.parameterInitialValue("alpha")[0], 0.15)
        self.assertEqual(config.parameterCalibration("alpha"), "Calibrated")
        self.assertEqual(config.maxCalibrationAttempts(), 7)
        self.assertEqual(config_copy.parameterCalibration("alpha"), "Calibrated")

    def test_price_segment_roundtrip(self):
        segment = ore.PriceSegment.withOffPeakDaily(
            "CMDTY_USD", ["OFFPEAK/1"], ["PEAK/1"]
        )
        segment_copy = self._assert_roundtrip(
            segment,
            ore.PriceSegment,
            ["CMDTY_USD", "OffPeakPowerDaily"],
        )

        self.assertEqual(segment.conventionsId(), "CMDTY_USD")
        self.assertEqual(segment_copy.conventionsId(), "CMDTY_USD")

    def test_adjustment_factors_smoke(self):
        factors = ore.AdjustmentFactors.create(ore.Date(10, ore.March, 2026))
        factors.addFactor("EQ-SP5", ore.Date(1, ore.March, 2026), 0.5)

        self.assertTrue(factors.hasFactor("EQ-SP5"))
        self.assertAlmostEqual(
            factors.getFactorContribution("EQ-SP5", ore.Date(1, ore.March, 2026)),
            0.5,
        )

        self.assertIn("EQ-SP5", factors.toXMLString())

    def test_report_and_currency_config_roundtrip(self):
        self._assert_roundtrip(ore.ReportConfig(), ore.ReportConfig)
        self._assert_roundtrip(ore.CurrencyConfig(), ore.CurrencyConfig)

    def test_default_curve_and_basel_helpers(self):
        config = ore.DefaultCurveConfig.fromSingleConfig(
            "DEFAULT_USD",
            "Default USD",
            "USD",
            "SpreadCDS",
            "USD-LIBOR-3M",
            "RECOVERY/ABC",
            ore.Actual365Fixed(),
            "USD-CDS",
        )
        self.assertEqual(config.currency(), "USD")

        basel = ore.BaselTrafficLightData()
        basel.setObservationData(250, [250], [5], [10])
        basel_copy = self._assert_roundtrip(
            basel,
            ore.BaselTrafficLightData,
            ["250", "4", "9"],
        )
        self.assertTrue(len(basel_copy.toXMLString()) > 0)

    def test_ibor_fallback_config_helpers_and_roundtrip(self):
        """Validate fallback data wrapping and convenience insertion."""
        usd_switch = ore.Date(15, ore.June, 2026)
        gbp_switch = ore.Date(16, ore.June, 2026)

        usd_fallback = ore.IborFallbackConfig.FallbackData(
            "SOFR", 0.002615, usd_switch
        )
        self.assertEqual(usd_fallback.rfrIndex, "SOFR")
        self.assertAlmostEqual(usd_fallback.spread, 0.002615)
        self.assertEqual(usd_fallback.switchDate, usd_switch)

        fallbacks = ore.StringFallbackDataMap()
        fallbacks["USD-LIBOR-3M"] = usd_fallback

        config = ore.IborFallbackConfig(True, True, False, fallbacks)
        config.addIndexFallbackRule(
            "GBP-LIBOR-6M", "SONIA", 0.001193, gbp_switch
        )

        copied_usd = config.fallbackData("USD-LIBOR-3M")
        copied_gbp = config.fallbackData("GBP-LIBOR-6M")
        self.assertEqual(copied_usd.rfrIndex, "SOFR")
        self.assertAlmostEqual(copied_gbp.spread, 0.001193)
        self.assertEqual(copied_gbp.switchDate, gbp_switch)

        config_copy = self._assert_roundtrip(
            config,
            ore.IborFallbackConfig,
            ["USD-LIBOR-3M", "SOFR", "GBP-LIBOR-6M", "SONIA"],
        )
        roundtripped_gbp = config_copy.fallbackData("GBP-LIBOR-6M")
        self.assertEqual(roundtripped_gbp.rfrIndex, "SONIA")
        self.assertAlmostEqual(roundtripped_gbp.spread, 0.001193)
        self.assertEqual(roundtripped_gbp.switchDate, gbp_switch)


if __name__ == '__main__':
    print('testing ORE configuration bindings')
    unittest.main()
