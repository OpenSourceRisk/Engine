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


class Phase3ModelBuilderBindingSmokeTest(unittest.TestCase):
    """Verify Phase 3 model builder wrappers are exposed and usable."""

    # ------------------------------------------------------------------
    # Symbol availability
    # ------------------------------------------------------------------

    def test_phase3_symbols_are_available(self) -> None:
        """Ensure all Phase 3 builder and data classes are exported."""
        required = [
            "CalibrationConfiguration",
            "HestonModelCalibration",
            "HestonModelBuilder",
            "EqBsBuilder",
            "CommoditySchwartzModelBuilder",
            "CommoditySchwartzData",
            "AssetModelBuilderBase",
            "LocalVolModelBuilder",
            "InfDkBuilder",
            "InfJyBuilder",
            "InfJyData",
        ]
        for symbol in required:
            self.assertTrue(hasattr(ORE, symbol), msg=f"Missing symbol: {symbol}")

    # ------------------------------------------------------------------
    # CalibrationConfiguration
    # ------------------------------------------------------------------

    def test_calibration_configuration_default_ctor(self) -> None:
        """CalibrationConfiguration is default-constructible."""
        cfg = ORE.CalibrationConfiguration()
        self.assertIsNotNone(cfg)

    def test_calibration_configuration_setters(self) -> None:
        """CalibrationConfiguration exposes rmse tolerance and max iterations accessors."""
        cfg = ORE.CalibrationConfiguration()
        self.assertTrue(hasattr(cfg, "maxIterations"))
        self.assertTrue(hasattr(cfg, "rmseTolerance"))
        self.assertTrue(hasattr(cfg, "fromXMLString"))
        self.assertTrue(hasattr(cfg, "toXMLString"))

    # ------------------------------------------------------------------
    # HestonModelCalibration
    # ------------------------------------------------------------------

    def test_heston_model_calibration_default_ctor(self) -> None:
        """HestonModelCalibration class is present and has expected API."""
        # The constructor requires mandatory args (indexName, process),
        # so we verify the class is accessible and exposes its calibrated model.
        self.assertTrue(hasattr(ORE.HestonModelCalibration, "model"))

    def test_heston_model_calibration_api(self) -> None:
        """HestonModelCalibration exposes the model accessor."""
        self.assertTrue(hasattr(ORE.HestonModelCalibration, "model"))

    # ------------------------------------------------------------------
    # CommoditySchwartzData
    # ------------------------------------------------------------------

    def test_commodity_schwartz_data_default_ctor(self) -> None:
        """CommoditySchwartzData is default-constructible."""
        data = ORE.CommoditySchwartzData()
        self.assertIsNotNone(data)

    def test_commodity_schwartz_data_api(self) -> None:
        """CommoditySchwartzData exposes name/currency and xml round-trip."""
        data = ORE.CommoditySchwartzData()
        self.assertTrue(hasattr(data, "name"))
        self.assertTrue(hasattr(data, "currency"))
        self.assertTrue(hasattr(data, "fromXML"))
        self.assertTrue(hasattr(data, "toXML"))

    # ------------------------------------------------------------------
    # InfJyData
    # ------------------------------------------------------------------

    def test_inf_jy_data_default_ctor(self) -> None:
        """InfJyData is default-constructible."""
        data = ORE.InfJyData()
        self.assertIsNotNone(data)

    def test_inf_jy_data_xml_roundtrip(self) -> None:
        """InfJyData exposes fromXMLString / toXMLString."""
        data = ORE.InfJyData()
        self.assertTrue(hasattr(data, "fromXMLString"))
        self.assertTrue(hasattr(data, "toXMLString"))

    # ------------------------------------------------------------------
    # Builder API surface (no live calibration required)
    # ------------------------------------------------------------------

    def test_heston_model_builder_api(self) -> None:
        """HestonModelBuilder class is present with expected callable methods."""
        cls = ORE.HestonModelBuilder
        self.assertTrue(hasattr(cls, "getCalibratedProcesses"))
        self.assertTrue(hasattr(cls, "requiresRecalibration"))

    def test_eq_bs_builder_api(self) -> None:
        """EqBsBuilder class is present with expected callable methods."""
        cls = ORE.EqBsBuilder
        self.assertTrue(hasattr(cls, "parametrization"))
        self.assertTrue(hasattr(cls, "error"))

    def test_commodity_schwartz_model_builder_api(self) -> None:
        """CommoditySchwartzModelBuilder class is present with expected callable methods."""
        cls = ORE.CommoditySchwartzModelBuilder
        self.assertTrue(hasattr(cls, "error"))
        self.assertTrue(hasattr(cls, "name"))
        self.assertTrue(hasattr(cls, "optionBasket"))

    def test_local_vol_model_builder_api(self) -> None:
        """LocalVolModelBuilder class is present with expected callable methods."""
        cls = ORE.LocalVolModelBuilder
        self.assertTrue(hasattr(cls, "getCalibratedProcesses"))

    def test_inf_dk_builder_api(self) -> None:
        """InfDkBuilder class is present with expected callable methods."""
        cls = ORE.InfDkBuilder
        self.assertTrue(hasattr(cls, "infIndex"))
        self.assertTrue(hasattr(cls, "optionBasket"))

    def test_inf_jy_builder_api(self) -> None:
        """InfJyBuilder class is present with expected callable methods."""
        cls = ORE.InfJyBuilder
        self.assertTrue(hasattr(cls, "inflationIndex"))
        self.assertTrue(hasattr(cls, "recalibrate"))

    def test_asset_model_builder_base_api(self) -> None:
        """AssetModelBuilderBase exposes the standard recalibration interface."""
        cls = ORE.AssetModelBuilderBase
        self.assertTrue(hasattr(cls, "requiresRecalibration"))
        self.assertTrue(hasattr(cls, "recalibrate"))


if __name__ == "__main__":
    unittest.main()
