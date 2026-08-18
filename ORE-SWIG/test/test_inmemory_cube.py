"""Regression tests for the wrongly abstract in-memory NPV cube classes.

``SinglePrecisionInMemoryCubeN`` and ``DoublePrecisionInMemoryCubeN`` used to
raise ``AttributeError: No constructor defined - class is abstract`` on
construction, leaving Python with no way to build an NPV cube at all.  The
overrides in the SWIG interface did not textually match the base's pure
virtuals -- they spelled the parameters ``QuantLib::Size`` (a type the SWIG
parser cannot resolve) and dropped the base's default arguments, which SWIG
expands into one required signature per arity -- so SWIG considered the pure
virtuals unimplemented and dropped the constructors.
"""

import unittest

import ORE


class InMemoryCubeConstructionTest(unittest.TestCase):
    """The in-memory cube instantiations must be concrete."""

    CUBE_CLASSES = (
        "SinglePrecisionInMemoryCubeN",
        "DoublePrecisionInMemoryCubeN",
    )

    @staticmethod
    def _cube_args():
        asof = ORE.Date(2, ORE.March, 2026)
        ids = {"trade1", "trade2"}
        dates = [ORE.Date(2, ORE.March, 2027), ORE.Date(2, ORE.March, 2028)]
        return asof, ids, dates, 4

    def test_cubes_construct(self) -> None:
        """Both precisions used to raise 'class is abstract' here."""
        asof, ids, dates, samples = self._cube_args()
        for name in self.CUBE_CLASSES:
            with self.subTest(cube=name):
                cube = getattr(ORE, name)(asof, ids, dates, samples)
                self.assertIsInstance(cube, ORE.NPVCube)
                self.assertEqual(cube.numIds(), 2)
                self.assertEqual(cube.numDates(), 2)
                self.assertEqual(cube.samples(), 4)
                self.assertEqual(str(cube.asof()), str(asof))

    def test_cube_set_get_roundtrip(self) -> None:
        """Values written into a constructed cube must read back."""
        asof, ids, dates, samples = self._cube_args()
        cube = ORE.DoublePrecisionInMemoryCubeN(asof, ids, dates, samples)
        cube.setT0(123.25, 0, 0)
        self.assertEqual(cube.getT0(0), 123.25)
        cube.set(7.5, 1, 1, 3, 0)
        self.assertEqual(cube.get(1, 1, 3), 7.5)


if __name__ == "__main__":
    unittest.main()
