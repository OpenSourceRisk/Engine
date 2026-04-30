"""Smoke tests for OREData model builder bindings."""

import unittest

import ORE


class OREDataModelBuilderBindingSmokeTest(unittest.TestCase):
    """Verify Task 6 model builder wrappers are exposed and usable."""

    def test_model_builder_symbols_are_available(self) -> None:
        """Ensure expected model builder classes are exported by the module."""
        required_symbols = [
            "ModelBuilder",
            "IrLgmData",
            "IrModelBuilder",
            "LgmBuilder",
            "HwBuilder",
            "FxBsBuilder",
            "CrossAssetModelBuilder",
        ]
        for symbol in required_symbols:
            self.assertTrue(hasattr(ORE, symbol), msg=f"Missing symbol: {symbol}")

    def test_model_builder_api_surface(self) -> None:
        """Check key methods are exposed for the new builder classes."""
        self.assertTrue(hasattr(ORE.IrModelBuilder, "requiresRecalibration"))
        self.assertTrue(hasattr(ORE.IrModelBuilder, "recalibrate"))
        self.assertTrue(hasattr(ORE.LgmBuilder, "error"))
        self.assertTrue(hasattr(ORE.HwBuilder, "error"))
        self.assertTrue(hasattr(ORE.FxBsBuilder, "parametrization"))
        self.assertTrue(hasattr(ORE.CrossAssetModelBuilder, "model"))

    def test_correlation_factor_and_builder_helpers(self) -> None:
        """Exercise the Python-friendly correlation helper surface."""
        factor_ir = ORE.CorrelationFactor("IR", "USD", 0)
        factor_fx = ORE.CorrelationFactor("FX", "EURUSD", 0)

        self.assertEqual(factor_ir.typeName(), "IR")
        self.assertEqual(factor_ir.name, "USD")
        self.assertEqual(factor_ir.index, 0)

        builder = ORE.CorrelationMatrixBuilder()
        builder.addCorrelationValue("IR", "USD", 0, "FX", "EURUSD", 0, 0.25)
        self.assertAlmostEqual(builder.correlationValue(factor_ir, factor_fx), 0.25)

        builder.addCorrelation("IR:USD:0", "FX:EURUSD:0", -0.5)
        self.assertAlmostEqual(
            builder.correlationValue("IR", "USD", 0, "FX", "EURUSD", 0),
            -0.5,
        )

    def test_cross_asset_model_correlation_helpers(self) -> None:
        """Verify InstantaneousCorrelations and CAM helpers are constructible."""
        factor_ir = ORE.CorrelationFactor("IR", "USD", 0)
        factor_eq = ORE.CorrelationFactor("EQ", "SP5", 0)
        factor_fx = ORE.CorrelationFactor("FX", "EURUSD", 0)

        correlations = ORE.InstantaneousCorrelations()
        correlations.setCorrelation(factor_ir, factor_eq, -0.15)
        self.assertAlmostEqual(correlations.correlationValue(factor_ir, factor_eq), -0.15)

        xml = correlations.toXMLString()
        self.assertIn("IR:USD", xml)
        self.assertIn("EQ:SP5", xml)

        clone = ORE.InstantaneousCorrelations()
        clone.fromXMLString(xml)
        self.assertAlmostEqual(
            clone.correlationValue("IR", "USD", 0, "EQ", "SP5", 0),
            -0.15,
        )

        model_data = ORE.CrossAssetModelData()
        model_data.setCorrelationData(correlations)
        self.assertAlmostEqual(
            model_data.correlationData().correlationValue(
                "IR", "USD", 0, "EQ", "SP5", 0
            ),
            -0.15,
        )
        self.assertAlmostEqual(model_data.correlationValue(factor_ir, factor_eq), -0.15)

        model_data.setCorrelationValue("IR", "USD", 0, "FX", "EURUSD", 0, 0.4)
        self.assertAlmostEqual(
            model_data.correlationValue(factor_ir, factor_fx),
            0.4,
        )


if __name__ == "__main__":
    unittest.main()
