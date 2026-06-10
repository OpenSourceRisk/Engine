"""Tests for DIM calculator stack SWIG bindings.

Validates that DynamicInitialMarginCalculator (expanded),
RegressionDynamicInitialMarginCalculator,
FlatDynamicInitialMarginCalculator, and
DirectDynamicInitialMarginCalculator are correctly exposed
in the ORE Python module (ACADIAQPR-14093).
"""

import unittest

import ORE


class DynamicInitialMarginCalculatorBindingTest(unittest.TestCase):
    """Validate expanded DynamicInitialMarginCalculator bindings."""

    _BASE_METHODS = [
        "build",
        "dimCube",
    ]

    _RESULT_ACCESSORS = [
        "dynamicIM",
        "cashFlow",
        "expectedIM",
        "currentIM",
        "getInitialMarginScaling",
    ]

    def test_class_is_exported(self) -> None:
        """Assert DynamicInitialMarginCalculator is in ORE."""
        self.assertTrue(
            hasattr(ORE, "DynamicInitialMarginCalculator"),
            msg="DynamicInitialMarginCalculator missing from ORE",
        )

    def test_base_methods_present(self) -> None:
        """Assert lifecycle methods are present."""
        for method in self._BASE_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.DynamicInitialMarginCalculator, method
                ),
                msg=(
                    "DynamicInitialMarginCalculator missing "
                    f"method: {method}"
                ),
            )

    def test_result_accessors_present(self) -> None:
        """Assert new result-accessor methods are present."""
        for method in self._RESULT_ACCESSORS:
            self.assertTrue(
                hasattr(
                    ORE.DynamicInitialMarginCalculator, method
                ),
                msg=(
                    "DynamicInitialMarginCalculator missing "
                    f"result accessor: {method}"
                ),
            )


class RegressionDIMCalculatorBindingTest(unittest.TestCase):
    """Validate RegressionDynamicInitialMarginCalculator."""

    _EXPECTED_METHODS = [
        "build",
        "unscaledCurrentDIM",
        "localRegressionResults",
        "zeroOrderResults",
        "simpleResultsUpper",
        "simpleResultsLower",
    ]

    _INHERITED_METHODS = [
        "dimCube",
        "dynamicIM",
        "cashFlow",
        "expectedIM",
        "currentIM",
        "getInitialMarginScaling",
    ]

    def test_class_is_exported(self) -> None:
        """Assert class is present in ORE module."""
        self.assertTrue(
            hasattr(
                ORE,
                "RegressionDynamicInitialMarginCalculator",
            ),
            msg=(
                "RegressionDynamicInitialMarginCalculator "
                "missing from ORE"
            ),
        )

    def test_own_methods_present(self) -> None:
        """Assert own methods are present."""
        for method in self._EXPECTED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.RegressionDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "RegressionDynamicInitialMarginCalculator "
                    f"missing method: {method}"
                ),
            )

    def test_inherited_methods_present(self) -> None:
        """Assert base-class result accessors are inherited."""
        for method in self._INHERITED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.RegressionDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "RegressionDynamicInitialMarginCalculator "
                    f"missing inherited method: {method}"
                ),
            )


class FlatDIMCalculatorBindingTest(unittest.TestCase):
    """Validate FlatDynamicInitialMarginCalculator."""

    _EXPECTED_METHODS = [
        "build",
        "unscaledCurrentDIM",
        "dimResults",
    ]

    _INHERITED_METHODS = [
        "dimCube",
        "dynamicIM",
        "cashFlow",
        "expectedIM",
        "currentIM",
        "getInitialMarginScaling",
    ]

    def test_class_is_exported(self) -> None:
        """Assert class is present in ORE module."""
        self.assertTrue(
            hasattr(
                ORE, "FlatDynamicInitialMarginCalculator"
            ),
            msg=(
                "FlatDynamicInitialMarginCalculator "
                "missing from ORE"
            ),
        )

    def test_own_methods_present(self) -> None:
        """Assert own methods are present."""
        for method in self._EXPECTED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.FlatDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "FlatDynamicInitialMarginCalculator "
                    f"missing method: {method}"
                ),
            )

    def test_inherited_methods_present(self) -> None:
        """Assert base-class result accessors are inherited."""
        for method in self._INHERITED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.FlatDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "FlatDynamicInitialMarginCalculator "
                    f"missing inherited method: {method}"
                ),
            )


class DirectDIMCalculatorBindingTest(unittest.TestCase):
    """Validate DirectDynamicInitialMarginCalculator."""

    _EXPECTED_METHODS = [
        "build",
        "unscaledCurrentDIM",
    ]

    _INHERITED_METHODS = [
        "dimCube",
        "dynamicIM",
        "cashFlow",
        "expectedIM",
        "currentIM",
        "getInitialMarginScaling",
    ]

    def test_class_is_exported(self) -> None:
        """Assert class is present in ORE module."""
        self.assertTrue(
            hasattr(
                ORE,
                "DirectDynamicInitialMarginCalculator",
            ),
            msg=(
                "DirectDynamicInitialMarginCalculator "
                "missing from ORE"
            ),
        )

    def test_own_methods_present(self) -> None:
        """Assert own methods are present."""
        for method in self._EXPECTED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.DirectDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "DirectDynamicInitialMarginCalculator "
                    f"missing method: {method}"
                ),
            )

    def test_inherited_methods_present(self) -> None:
        """Assert base-class result accessors are inherited."""
        for method in self._INHERITED_METHODS:
            self.assertTrue(
                hasattr(
                    ORE.DirectDynamicInitialMarginCalculator,
                    method,
                ),
                msg=(
                    "DirectDynamicInitialMarginCalculator "
                    f"missing inherited method: {method}"
                ),
            )


class DIMCalculatorSymbolSmokeTest(unittest.TestCase):
    """Quick symbol-availability smoke test for all DIM types."""

    def test_all_dim_symbols_available(self) -> None:
        """Assert all ACADIAQPR-14093 symbols are in ORE."""
        required = [
            "DynamicInitialMarginCalculator",
            "RegressionDynamicInitialMarginCalculator",
            "FlatDynamicInitialMarginCalculator",
            "DirectDynamicInitialMarginCalculator",
        ]
        for name in required:
            self.assertTrue(
                hasattr(ORE, name),
                msg=f"Missing ORE symbol: {name}",
            )


if __name__ == "__main__":
    unittest.main()
