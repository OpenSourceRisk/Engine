"""Tests for ExposureCalculator and NettedExposureCalculator SWIG bindings.

Validates that ACADIAQPR-14063 result-accessor methods and the
NettedExposureCalculator wrapper are correctly exposed in the ORE Python module.
"""

import unittest

import ORE


class ExposureCalculatorBindingTest(unittest.TestCase):
    """Validate ExposureCalculator result-accessor methods are exposed."""

    # Expected methods added by ACADIAQPR-14063
    _METADATA_METHODS = [
        "dates",
        "today",
        "nettingSetIds",
        "nettingSetValueToday",
        "nettingSetMaturity",
        "times",
        "baseCurrency",
        "portfolio",
        "npvCube",
        "market",
        "exposureCube",
    ]

    _PROFILE_METHODS = [
        "epe",
        "ene",
        "allocatedEpe",
        "allocatedEne",
        "ee_b",
        "eee_b",
        "pfe",
        "epe_b",
        "eepe_b",
        "epe_b_timeWeighted",
        "eepe_b_timeWeighted",
    ]

    def test_class_is_exported(self) -> None:
        """Assert ExposureCalculator is present in the ORE module."""
        self.assertTrue(
            hasattr(ORE, "ExposureCalculator"),
            msg="ExposureCalculator missing from ORE module",
        )

    def test_metadata_methods_present(self) -> None:
        """Assert all metadata accessor methods are present on ExposureCalculator."""
        for method in self._METADATA_METHODS:
            self.assertTrue(
                hasattr(ORE.ExposureCalculator, method),
                msg=f"ExposureCalculator missing method: {method}",
            )

    def test_profile_methods_present(self) -> None:
        """Assert all exposure profile accessor methods are present."""
        for method in self._PROFILE_METHODS:
            self.assertTrue(
                hasattr(ORE.ExposureCalculator, method),
                msg=f"ExposureCalculator missing profile method: {method}",
            )


class NettedExposureCalculatorBindingTest(unittest.TestCase):
    """Validate NettedExposureCalculator wrapper and nested struct are exposed."""

    _PROFILE_METHODS = [
        "build",
        "exposureCube",
        "nettedCube",
        "epe",
        "ene",
        "ee_b",
        "eee_b",
        "pfe",
        "epe_b",
        "eepe_b",
        "epe_b_timeWeighted",
        "eepe_b_timeWeighted",
        "expectedCollateral",
        "colva",
        "colvaIncrements",
        "collateralFloor",
        "collateralFloorIncrements",
        "counterpartyMap",
    ]

    _TAE_FIELDS = [
        "positiveExposureBeforeCollateral",
        "negativeExposureBeforeCollateral",
        "positiveExposureAfterCollateral",
        "negativeExposureAfterCollateral",
    ]

    def test_class_is_exported(self) -> None:
        """Assert NettedExposureCalculator is present in the ORE module."""
        self.assertTrue(
            hasattr(ORE, "NettedExposureCalculator"),
            msg="NettedExposureCalculator missing from ORE module",
        )

    def test_profile_methods_present(self) -> None:
        """Assert all profile and collateral accessor methods are present."""
        for method in self._PROFILE_METHODS:
            self.assertTrue(
                hasattr(ORE.NettedExposureCalculator, method),
                msg=f"NettedExposureCalculator missing method: {method}",
            )

    def test_time_averaged_exposure_struct_exported(self) -> None:
        """Assert nested TimeAveragedExposure struct is exported as a flat type."""
        self.assertTrue(
            hasattr(ORE, "NettedExposureCalculatorTimeAveragedExposure"),
            msg=(
                "NettedExposureCalculatorTimeAveragedExposure missing from ORE module"
            ),
        )

    def test_time_averaged_exposure_fields_present(self) -> None:
        """Assert TimeAveragedExposure struct has all four exposure fields."""
        tae = ORE.NettedExposureCalculatorTimeAveragedExposure
        for field in self._TAE_FIELDS:
            self.assertTrue(
                hasattr(tae, field),
                msg=(
                    f"NettedExposureCalculatorTimeAveragedExposure "
                    f"missing field: {field}"
                ),
            )


if __name__ == "__main__":
    unittest.main()
