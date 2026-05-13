"""Tests for CVASpreadSensitivityCalculator and ExposureAllocator
SWIG bindings (ACADIAQPR-14094).

Validates that all newly wrapped aggregation classes, enums,
and free functions are accessible from Python.
"""

import unittest

import ORE


class CVASpreadSensitivityCalculatorBindingTest(unittest.TestCase):
    """Validate CVASpreadSensitivityCalculator SWIG binding."""

    _INSPECTOR_METHODS = [
        "key",
        "asof",
        "exposureProfile",
        "exposureDateGrid",
        "recoveryRate",
        "shiftTenors",
    ]

    _RESULT_METHODS = [
        "shiftTimes",
        "shiftSize",
        "hazardRateSensitivities",
        "cdsSpreadSensitivities",
    ]

    def test_class_is_exported(self) -> None:
        """CVASpreadSensitivityCalculator must exist in ORE."""
        self.assertTrue(
            hasattr(ORE, "CVASpreadSensitivityCalculator"),
            msg="CVASpreadSensitivityCalculator missing from ORE",
        )

    def test_inspector_methods_present(self) -> None:
        """All inspector methods must be on the class."""
        for name in self._INSPECTOR_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.CVASpreadSensitivityCalculator, name
                ),
                msg=(
                    "CVASpreadSensitivityCalculator "
                    f"missing inspector: {name}"
                ),
            )

    def test_result_methods_present(self) -> None:
        """All result accessor methods must be on the class."""
        for name in self._RESULT_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.CVASpreadSensitivityCalculator, name
                ),
                msg=(
                    "CVASpreadSensitivityCalculator "
                    f"missing result accessor: {name}"
                ),
            )


class ExposureAllocatorBindingTest(unittest.TestCase):
    """Validate ExposureAllocator base class and enum binding."""

    def test_class_is_exported(self) -> None:
        """ExposureAllocator must exist in ORE."""
        self.assertTrue(
            hasattr(ORE, "ExposureAllocator"),
            msg="ExposureAllocator missing from ORE",
        )

    def test_build_method_present(self) -> None:
        """ExposureAllocator must expose build()."""
        self.assertTrue(
            hasattr(ORE.ExposureAllocator, "build"),
            msg="ExposureAllocator missing build()",
        )

    def test_exposure_cube_method_present(self) -> None:
        """ExposureAllocator must expose exposureCube()."""
        self.assertTrue(
            hasattr(ORE.ExposureAllocator, "exposureCube"),
            msg="ExposureAllocator missing exposureCube()",
        )

    def test_allocation_method_enum_values(self) -> None:
        """AllocationMethod enum values must be accessible."""
        expected = [
            "AllocationMethod__None",
            "AllocationMethod_Marginal",
            "AllocationMethod_RelativeFairValueGross",
            "AllocationMethod_RelativeFairValueNet",
            "AllocationMethod_RelativeXVA",
        ]
        for val in expected:
            self.assertTrue(
                hasattr(ORE.ExposureAllocator, val),
                msg=(
                    "ExposureAllocator missing enum: "
                    f"{val}"
                ),
            )


class ExposureAllocatorSubclassesBindingTest(unittest.TestCase):
    """Validate four concrete ExposureAllocator subclasses."""

    _SUBCLASSES = [
        "RelativeFairValueNetExposureAllocator",
        "RelativeFairValueGrossExposureAllocator",
        "RelativeXvaExposureAllocator",
        "NoneExposureAllocator",
    ]

    def test_subclasses_exported(self) -> None:
        """All four subclasses must be in the ORE module."""
        for name in self._SUBCLASSES:
            self.assertTrue(
                hasattr(ORE, name),
                msg=f"Missing ORE symbol: {name}",
            )

    def test_subclasses_inherit_build(self) -> None:
        """Subclasses must inherit build() from base."""
        for name in self._SUBCLASSES:
            cls = getattr(ORE, name)
            self.assertTrue(
                hasattr(cls, "build"),
                msg=f"{name} missing inherited build()",
            )

    def test_subclasses_inherit_exposure_cube(self) -> None:
        """Subclasses must inherit exposureCube() from base."""
        for name in self._SUBCLASSES:
            cls = getattr(ORE, name)
            self.assertTrue(
                hasattr(cls, "exposureCube"),
                msg=(
                    f"{name} missing inherited "
                    "exposureCube()"
                ),
            )


class ParseAllocationMethodBindingTest(unittest.TestCase):
    """Validate parseAllocationMethod free function."""

    def test_function_is_exported(self) -> None:
        """parseAllocationMethod must exist in ORE."""
        self.assertTrue(
            hasattr(ORE, "parseAllocationMethod"),
            msg="parseAllocationMethod missing from ORE",
        )


class Ticket6SymbolSmokeTest(unittest.TestCase):
    """Quick symbol-availability smoke test for ACADIAQPR-14094."""

    def test_all_new_symbols_available(self) -> None:
        """All ticket 6 symbols must be in the ORE module."""
        required = [
            "CVASpreadSensitivityCalculator",
            "ExposureAllocator",
            "RelativeFairValueNetExposureAllocator",
            "RelativeFairValueGrossExposureAllocator",
            "RelativeXvaExposureAllocator",
            "NoneExposureAllocator",
            "parseAllocationMethod",
        ]
        for name in required:
            self.assertTrue(
                hasattr(ORE, name),
                msg=f"Missing ORE symbol: {name}",
            )


if __name__ == "__main__":
    unittest.main()
