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


if __name__ == "__main__":
    unittest.main()
