"""
Copyright (C) 2026 Quaternion Risk Management Ltd
All rights reserved.
"""

from ORE import *
import unittest


class QleTask522IndexesSmokeTest(unittest.TestCase):
    def setUp(self):
        self.today = Date(2, March, 2026)
        Settings.instance().evaluationDate = self.today
        self.curve = RelinkableYieldTermStructureHandle()
        self.curve.linkTo(FlatForward(self.today, 0.02, Actual360()))
        self.switch_date = Date(1, January, 2027)

    def test_symbols_available(self):
        ore_module = __import__("ORE")
        expected = [
            "USDAmbor",
            "USDAmeribor",
            "BOEBaseRateIndex",
            "CHFSaron",
            "CNHHibor",
            "CNHShibor",
            "CNYRepoFix",
            "DKKCita",
            "FallbackIborIndex",
            "FallbackOvernightIndex",
            "HKDHonia",
            "IDRJibor",
            "ILSTelbor",
            "INRMiborOis",
            "JPYEYTIBOR",
            "KRWCd",
            "Nowa",
            "PLNPolonia",
            "PrimeIndex",
            "RUBKeyRate",
            "SAibor",
            "SEKStina",
            "SofrTerm",
            "SoniaTerm",
            "Sora",
            "TermRateIndex",
            "THBThor",
            "TonarTerm",
        ]

        for class_name in expected:
            self.assertTrue(hasattr(ore_module, class_name), class_name)

    def test_term_and_fallback_index_construction(self):
        sofr_term = SofrTerm(Period(3, Months), self.curve)
        self.assertIn("SOFRTerm", sofr_term.name())
        self.assertIn("SOFR", sofr_term.rfrIndex().name())

        sonia_term = SoniaTerm(Period(1, Months), self.curve)
        self.assertIn("SONIATerm", sonia_term.name())
        self.assertIn("Sonia", sonia_term.rfrIndex().name())

        tonar_term = TonarTerm(Period(6, Months), self.curve)
        self.assertIn("TONARTerm", tonar_term.name())
        self.assertIn("Tonar", tonar_term.rfrIndex().name())

        sora = Sora(self.curve)
        self.assertIn("SORA", sora.name())

        original_ibor = USDAmbor(Period(1, Months), self.curve)
        fallback_ibor = FallbackIborIndex(
            original_ibor,
            Sofr(self.curve),
            0.001,
            self.switch_date,
            True,
        )
        self.assertEqual(fallback_ibor.originalIndex().name(), original_ibor.name())
        self.assertIn("SOFR", fallback_ibor.rfrIndex().name())
        self.assertEqual(fallback_ibor.switchDate(), self.switch_date)
        self.assertAlmostEqual(fallback_ibor.spread(), 0.001)

        original_overnight = USDAmeribor(self.curve)
        fallback_overnight = FallbackOvernightIndex(
            original_overnight,
            Sofr(self.curve),
            0.0,
            self.switch_date,
            True,
        )
        self.assertEqual(fallback_overnight.originalIndex().name(), original_overnight.name())
        self.assertIn("SOFR", fallback_overnight.rfrIndex().name())
        self.assertEqual(fallback_overnight.switchDate(), self.switch_date)


if __name__ == "__main__":
    unittest.main()
