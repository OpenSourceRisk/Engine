"""Regression tests for https://github.com/OpenSourceRisk/Engine/issues/351.

SWIG registers its smart-pointer machinery under the bare namespace ``ext``, so
a declaration spelled ``QuantLib::ext::shared_ptr<T>`` used to be registered as
a type distinct from ``ext::shared_ptr<T>`` and wrapped as an opaque pointer.
Setters written with the qualified spelling then rejected every value the
bindings could produce -- ``InputParameters.setCube`` could not be handed the
cube returned by ``OREApp.getCube``, which made in-memory cube reuse
unreachable from Python.
"""

import unittest

import ORE


class QualifiedSharedPtrSetterTest(unittest.TestCase):
    """Setters declared with the qualified spelling must accept wrapped objects."""

    #: Setters that take a qualified ``shared_ptr`` whose argument type can be
    #: constructed directly from Python.
    ENGINE_DATA_SETTERS = (
        "setXvaSensiPricingEngine",
        "setParConversionPricingEngine",
        "setParStressPricingEngine",
        "setZeroToParShiftPricingEngine",
    )

    def test_engine_data_setters_accept_a_wrapped_engine_data(self) -> None:
        """These rejected every EngineData the bindings could produce."""
        params = ORE.InputParameters()
        for name in self.ENGINE_DATA_SETTERS:
            if not hasattr(params, name):
                continue  # setter predates this release
            with self.subTest(setter=name):
                getattr(params, name)(ORE.EngineData())

    def test_set_cube_accepts_a_wrapped_cube(self) -> None:
        """The round-trip issue #351 is about: hand a live cube to setCube.

        Building the cube needs the in-memory cube classes to be concrete
        (issue #354); where they are still abstract the round-trip cannot be
        exercised and the test is skipped rather than failed.

        setMarketCube is fixed by the same change but stays untested here:
        AggregationScenarioData has no constructible subclass in the bindings.
        """
        asof = ORE.Date(2, ORE.March, 2026)
        dates = [ORE.Date(2, ORE.March, 2027), ORE.Date(2, ORE.March, 2028)]
        try:
            cube = ORE.DoublePrecisionInMemoryCubeN(asof, {"trade1"}, dates, 4)
        except AttributeError:
            self.skipTest("in-memory cubes not constructible here (issue #354)")
        params = ORE.InputParameters()
        params.setCube(cube)


if __name__ == "__main__":
    unittest.main()
