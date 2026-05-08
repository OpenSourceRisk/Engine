"""Tests for XvaCalculator, StaticCreditXvaCalculator, and DynamicCreditXvaCalculator
SWIG bindings.
"""

import unittest

import ORE


class XvaCalculatorBindingTest(unittest.TestCase):
    """Validate XvaCalculator (ValueAdjustmentCalculator) result-accessor methods."""

    _METADATA_METHODS = [
        "build",
        "dates",
        "asof",
    ]

    # No-arg overloads returning full result maps (disambiguated via %rename)
    _MAP_METHODS = [
        "tradeCvaMap",
        "tradeDvaMap",
        "nettingSetCvaMap",
        "nettingSetDvaMap",
        "nettingSetSumCvaMap",
        "nettingSetSumDvaMap",
    ]

    # Per-trade scalar accessors (single-string-arg overloads)
    _TRADE_SCALAR_METHODS = [
        "tradeCva",
        "tradeDva",
        "tradeFba",
        "tradeFba_exOwnSp",
        "tradeFba_exAllSp",
        "tradeFca",
        "tradeFca_exOwnSp",
        "tradeFca_exAllSp",
        "tradeMva",
    ]

    # Per-netting-set scalar accessors (single-string-arg overloads)
    _NETTING_SET_SCALAR_METHODS = [
        "nettingSetCva",
        "nettingSetDva",
        "nettingSetFba",
        "nettingSetFba_exOwnSp",
        "nettingSetFba_exAllSp",
        "nettingSetFca",
        "nettingSetFca_exOwnSp",
        "nettingSetFca_exAllSp",
        "nettingSetMva",
        "nettingSetSumCva",
        "nettingSetSumDva",
    ]

    def test_class_is_exported(self) -> None:
        """Assert XvaCalculator is present in the ORE module."""
        self.assertTrue(
            hasattr(ORE, "XvaCalculator"),
            msg="XvaCalculator missing from ORE module",
        )

    def test_metadata_methods_present(self) -> None:
        """Assert metadata and lifecycle methods are present on XvaCalculator."""
        for method in self._METADATA_METHODS:
            self.assertTrue(
                hasattr(ORE.XvaCalculator, method),
                msg=f"XvaCalculator missing method: {method}",
            )

    def test_map_accessor_methods_present(self) -> None:
        """Assert renamed no-arg map accessors are present on XvaCalculator."""
        for method in self._MAP_METHODS:
            self.assertTrue(
                hasattr(ORE.XvaCalculator, method),
                msg=f"XvaCalculator missing map accessor: {method}",
            )

    def test_trade_scalar_methods_present(self) -> None:
        """Assert per-trade scalar accessor methods are present on XvaCalculator."""
        for method in self._TRADE_SCALAR_METHODS:
            self.assertTrue(
                hasattr(ORE.XvaCalculator, method),
                msg=f"XvaCalculator missing per-trade scalar: {method}",
            )

    def test_netting_set_scalar_methods_present(self) -> None:
        """Assert per-netting-set scalar accessors are present on XvaCalculator."""
        for method in self._NETTING_SET_SCALAR_METHODS:
            self.assertTrue(
                hasattr(ORE.XvaCalculator, method),
                msg=f"XvaCalculator missing per-netting-set scalar: {method}",
            )


class StaticCreditXvaCalculatorBindingTest(unittest.TestCase):
    """Validate StaticCreditXvaCalculator subclass is exported and inherits accessors."""

    def test_class_is_exported(self) -> None:
        """Assert StaticCreditXvaCalculator is present in the ORE module."""
        self.assertTrue(
            hasattr(ORE, "StaticCreditXvaCalculator"),
            msg="StaticCreditXvaCalculator missing from ORE module",
        )

    def test_inherits_map_accessors(self) -> None:
        """Assert StaticCreditXvaCalculator inherits no-arg map accessors."""
        inherited = [
            "tradeCvaMap",
            "tradeDvaMap",
            "nettingSetCvaMap",
            "nettingSetDvaMap",
            "nettingSetSumCvaMap",
            "nettingSetSumDvaMap",
        ]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.StaticCreditXvaCalculator, method),
                msg=f"StaticCreditXvaCalculator missing inherited accessor: {method}",
            )

    def test_inherits_trade_scalar_accessors(self) -> None:
        """Assert StaticCreditXvaCalculator inherits per-trade scalar accessors."""
        inherited = ["tradeCva", "tradeDva", "tradeFba", "tradeFca", "tradeMva"]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.StaticCreditXvaCalculator, method),
                msg=f"StaticCreditXvaCalculator missing inherited scalar: {method}",
            )

    def test_inherits_netting_set_scalar_accessors(self) -> None:
        """Assert StaticCreditXvaCalculator inherits per-netting-set scalar accessors."""
        inherited = ["nettingSetCva", "nettingSetDva", "nettingSetMva", "nettingSetSumCva"]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.StaticCreditXvaCalculator, method),
                msg=f"StaticCreditXvaCalculator missing inherited netting-set scalar: {method}",
            )


class DynamicCreditXvaCalculatorBindingTest(unittest.TestCase):
    """Validate DynamicCreditXvaCalculator subclass is exported and inherits accessors."""

    def test_class_is_exported(self) -> None:
        """Assert DynamicCreditXvaCalculator is present in the ORE module."""
        self.assertTrue(
            hasattr(ORE, "DynamicCreditXvaCalculator"),
            msg="DynamicCreditXvaCalculator missing from ORE module",
        )

    def test_inherits_map_accessors(self) -> None:
        """Assert DynamicCreditXvaCalculator inherits no-arg map accessors."""
        inherited = [
            "tradeCvaMap",
            "tradeDvaMap",
            "nettingSetCvaMap",
            "nettingSetDvaMap",
            "nettingSetSumCvaMap",
            "nettingSetSumDvaMap",
        ]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.DynamicCreditXvaCalculator, method),
                msg=f"DynamicCreditXvaCalculator missing inherited accessor: {method}",
            )

    def test_inherits_trade_scalar_accessors(self) -> None:
        """Assert DynamicCreditXvaCalculator inherits per-trade scalar accessors."""
        inherited = ["tradeCva", "tradeDva", "tradeFba", "tradeFca", "tradeMva"]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.DynamicCreditXvaCalculator, method),
                msg=f"DynamicCreditXvaCalculator missing inherited scalar: {method}",
            )

    def test_inherits_netting_set_scalar_accessors(self) -> None:
        """Assert DynamicCreditXvaCalculator inherits per-netting-set scalar accessors."""
        inherited = ["nettingSetCva", "nettingSetDva", "nettingSetMva", "nettingSetSumCva"]
        for method in inherited:
            self.assertTrue(
                hasattr(ORE.DynamicCreditXvaCalculator, method),
                msg=f"DynamicCreditXvaCalculator missing inherited netting-set scalar: {method}",
            )


class XvaCalculatorSymbolSmokeTest(unittest.TestCase):
    """Quick symbol-availability smoke test covering all new XVA calculator exports."""

    def test_all_new_symbols_available(self) -> None:
        """Assert all ACADIAQPR-14074 symbols are present in the ORE module."""
        required = [
            "XvaCalculator",
            "StaticCreditXvaCalculator",
            "DynamicCreditXvaCalculator",
        ]
        for name in required:
            self.assertTrue(
                hasattr(ORE, name),
                msg=f"Missing ORE symbol: {name}",
            )


if __name__ == "__main__":
    unittest.main()
