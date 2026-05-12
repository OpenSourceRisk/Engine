"""Smoke tests for OREAnalytics aggregation bindings."""

import unittest

import ORE


class AggregationBindingSmokeTest(unittest.TestCase):
    """Validate presence of Task 7 aggregation symbols."""

    def test_aggregation_symbols_available(self) -> None:
        """Assert that major aggregation-layer types are wrapped."""
        required = [
            "PostProcess",
            "ExposureCalculator",
            "DynamicInitialMarginCalculator",
            "CollateralAccount",
            "XvaCalculator",
        ]
        for name in required:
            self.assertTrue(hasattr(ORE, name), msg=f"Missing ORE symbol: {name}")

    def test_collateral_account_constructs(self) -> None:
        """Construct default collateral account."""
        account = ORE.CollateralAccount()
        self.assertIsNotNone(account)


class PostProcessAccessorSmokeTest(unittest.TestCase):
    """Validate that all PostProcess result-accessor methods are exposed (ACADIAQPR-14087)."""

    # Trade-level exposure profiles
    _TRADE_EXPOSURE_METHODS = [
        "tradeEPE",
        "tradeENE",
        "tradeEE_B",
        "tradeEPE_B",
        "tradeEEE_B",
        "tradeEEPE_B",
        "tradePFE",
        "tradeEPE_B_timeWeighted",
        "tradeEEPE_B_timeWeighted",
    ]

    # Netting-set-level exposure profiles
    _NET_EXPOSURE_METHODS = [
        "netEPE",
        "netENE",
        "netEE_B",
        "netEPE_B",
        "netEEE_B",
        "netEEPE_B",
        "netPFE",
        "netEPE_B_timeWeighted",
        "netEEPE_B_timeWeighted",
        "expectedCollateral",
        "colvaIncrements",
        "collateralFloorIncrements",
    ]

    # Allocated exposure profiles
    _ALLOCATED_EXPOSURE_METHODS = [
        "allocatedTradeEPE",
        "allocatedTradeENE",
    ]

    # Trade-level XVA scalars
    _TRADE_XVA_METHODS = [
        "tradeCVA",
        "tradeDVA",
        "tradeMVA",
        "tradeFBA",
        "tradeFCA",
        "tradeFBA_exOwnSP",
        "tradeFCA_exOwnSP",
        "tradeFBA_exAllSP",
        "tradeFCA_exAllSP",
        "allocatedTradeCVA",
        "allocatedTradeDVA",
    ]

    # Netting-set-level XVA scalars
    _NET_XVA_METHODS = [
        "nettingSetCVA",
        "nettingSetDVA",
        "nettingSetMVA",
        "nettingSetFBA",
        "nettingSetFCA",
        "nettingSetOurKVACCR",
        "nettingSetTheirKVACCR",
        "nettingSetOurKVACVA",
        "nettingSetTheirKVACVA",
        "nettingSetFBA_exOwnSP",
        "nettingSetFCA_exOwnSP",
        "nettingSetFBA_exAllSP",
        "nettingSetFCA_exAllSP",
        "nettingSetCOLVA",
        "nettingSetCollateralFloor",
    ]

    # CVA sensitivity inspectors
    _CVA_SENSI_METHODS = [
        "netCvaHazardRateSensitivity",
        "netCvaSpreadSensitivity",
        "spreadSensitivityTimes",
        "spreadSensitivityGrid",
        "cvaSpreadSensiShiftSize",
    ]

    # Other inspectors
    _OTHER_METHODS = [
        "tradeIds",
        "nettingSetIds",
        "counterpartyId",
        "portfolio",
        "cptyCube",
        "cube",
        "netCube",
    ]

    def _all_methods(self):
        return (
            self._TRADE_EXPOSURE_METHODS
            + self._NET_EXPOSURE_METHODS
            + self._ALLOCATED_EXPOSURE_METHODS
            + self._TRADE_XVA_METHODS
            + self._NET_XVA_METHODS
            + self._CVA_SENSI_METHODS
            + self._OTHER_METHODS
        )

    def test_postprocess_class_exists(self) -> None:
        """PostProcess class must be importable from ORE."""
        self.assertTrue(hasattr(ORE, "PostProcess"), msg="ORE.PostProcess not found")

    def test_postprocess_trade_exposure_methods(self) -> None:
        """All trade-level exposure accessor methods must be present on PostProcess."""
        for name in self._TRADE_EXPOSURE_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing trade exposure method: {name}",
            )

    def test_postprocess_net_exposure_methods(self) -> None:
        """All netting-set-level exposure accessor methods must be present on PostProcess."""
        for name in self._NET_EXPOSURE_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing net exposure method: {name}",
            )

    def test_postprocess_allocated_exposure_methods(self) -> None:
        """Allocated exposure accessor methods must be present on PostProcess."""
        for name in self._ALLOCATED_EXPOSURE_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing allocated exposure method: {name}",
            )

    def test_postprocess_trade_xva_methods(self) -> None:
        """All trade-level XVA scalar accessor methods must be present on PostProcess."""
        for name in self._TRADE_XVA_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing trade XVA scalar method: {name}",
            )

    def test_postprocess_net_xva_methods(self) -> None:
        """All netting-set-level XVA scalar accessor methods must be present on PostProcess."""
        for name in self._NET_XVA_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing netting-set XVA scalar method: {name}",
            )

    def test_postprocess_cva_sensitivity_methods(self) -> None:
        """CVA spread sensitivity inspector methods must be present on PostProcess."""
        for name in self._CVA_SENSI_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing CVA sensitivity method: {name}",
            )

    def test_postprocess_other_inspectors(self) -> None:
        """Other inspector methods (tradeIds, nettingSetIds, etc.) must be present on PostProcess."""
        for name in self._OTHER_METHODS:
            self.assertTrue(
                hasattr(ORE.PostProcess, name),
                msg=f"PostProcess missing inspector method: {name}",
            )

    def test_stringsize_map_type_exists(self) -> None:
        """StringSizeMap template must be exposed in ORE module (needed for tradeIds/nettingSetIds)."""
        self.assertTrue(hasattr(ORE, "StringSizeMap"), msg="ORE.StringSizeMap not found")


if __name__ == "__main__":
    unittest.main()
