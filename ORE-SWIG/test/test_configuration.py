"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest


class ConfigurationBindingsTest(unittest.TestCase):

    def test_bootstrap_config_roundtrip(self):
        cfg = BootstrapConfig(1.0e-10, 1.0e-10, False, 4, 2.5, 1.5, 8, False, 0.0)
        xml = cfg.toXMLString()
        cfg2 = BootstrapConfig()
        cfg2.fromXMLString(xml)
        self.assertTrue(len(cfg2.toXMLString()) > 0)

    def test_security_config_api_exposed(self):
        self.assertTrue(hasattr(SecurityConfig, "fromXML"))
        self.assertTrue(hasattr(SecurityConfig, "toXML"))

    def test_configuration_gap_types_are_exposed(self):
        self.assertTrue(hasattr(PriceSegment, "withOffPeakDaily"))
        self.assertTrue(hasattr(ParametricSmileConfiguration, "fromData"))
        self.assertTrue(hasattr(DefaultCurveConfig, "fromSingleConfig"))
        self.assertTrue(hasattr(BaselTrafficLightData, "setObservationData"))

    def test_parametric_smile_configuration_helpers(self):
        initial_values = DoubleVectorVector()
        initial_values.push_back([0.15])

        config = ParametricSmileConfiguration.fromData(
            ["alpha"], initial_values, ["Calibrated"], 7, 0.001, 0.01
        )
        self.assertEqual(config.parameterInitialValue("alpha")[0], 0.15)
        self.assertEqual(config.parameterCalibration("alpha"), "Calibrated")
        self.assertEqual(config.maxCalibrationAttempts(), 7)

    def test_price_segment_and_adjustment_factors_smoke(self):
        segment = PriceSegment.withOffPeakDaily("CMDTY_USD", ["OFFPEAK/1"], ["PEAK/1"])
        self.assertEqual(segment.conventionsId(), "CMDTY_USD")

        factors = AdjustmentFactors.create(Date(10, March, 2026))
        factors.addFactor("EQ-SP5", Date(1, March, 2026), 0.5)
        self.assertTrue(factors.hasFactor("EQ-SP5"))
        self.assertAlmostEqual(
            factors.getFactorContribution("EQ-SP5", Date(1, March, 2026)), 0.5
        )

    def test_report_and_currency_config_roundtrip(self):
        report_config = ReportConfig()
        report_xml = report_config.toXMLString()
        report_copy = ReportConfig()
        report_copy.fromXMLString(report_xml)
        self.assertTrue(len(report_copy.toXMLString()) > 0)

        currency_config = CurrencyConfig()
        currency_xml = currency_config.toXMLString()
        currency_copy = CurrencyConfig()
        currency_copy.fromXMLString(currency_xml)
        self.assertTrue(len(currency_copy.toXMLString()) > 0)

    def test_default_curve_and_basel_helpers(self):
        config = DefaultCurveConfig.fromSingleConfig(
            "DEFAULT_USD", "Default USD", "USD", "SpreadCDS", "USD-LIBOR-3M",
            "RECOVERY/ABC", Actual365Fixed(), "USD-CDS"
        )
        self.assertEqual(config.currency(), "USD")

        basel = BaselTrafficLightData()
        basel.setObservationData(250, [250], [5], [10])
        self.assertTrue(len(basel.toXMLString()) > 0)


if __name__ == '__main__':
    print('testing ORE configuration bindings')
    unittest.main()
