
"""
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest

class CorrelationRateQuoteTest(unittest.TestCase):

    def setUp(self):
        self.d = Date(1, January, 1990)
        self.value = 1
        self.input = "CORRELATION/RATE/INDEX1/INDEX2/1Y/ATM"
        self.datum = parseMarketDatum(self.d, self.input, self.value)

    def testSimpleInspectors(self):
        """ Test CorrelationQuote simple inspectors. """
        self.assertEqual(self.datum.asofDate(), self.d)
        self.assertAlmostEqual(self.datum.quote().value(), self.value)
        self.assertAlmostEqual(self.datum.instrumentType(), MarketDatum.InstrumentType_CORRELATION)
        self.assertAlmostEqual(self.datum.quoteType(), MarketDatum.QuoteType_RATE)

        corr = CorrelationQuote.getFullView(self.datum)
        self.assertEqual(corr.index1(), "INDEX1")
        self.assertEqual(corr.index2(), "INDEX2")
        self.assertEqual(corr.expiry(), "1Y")
        self.assertEqual(corr.strike(), "ATM")

class CorrelationPriceQuoteTest(unittest.TestCase):

    def setUp(self):
        self.d = Date(3, March, 2018)
        self.value = 10
        self.input = "CORRELATION/PRICE/INDEX1/INDEX2/1Y/0.1"
        self.datum = parseMarketDatum(self.d, self.input, self.value)

    def testSimpleInspectors(self):
        """ Test CorrelationQuote simple inspectors. """
        self.assertEqual(self.datum.asofDate(), self.d)
        self.assertAlmostEqual(self.datum.quote().value(), self.value)
        self.assertAlmostEqual(self.datum.instrumentType(), MarketDatum.InstrumentType_CORRELATION)
        self.assertAlmostEqual(self.datum.quoteType(), MarketDatum.QuoteType_PRICE)

        corr = CorrelationQuote.getFullView(self.datum)
        self.assertEqual(corr.index1(), "INDEX1")
        self.assertEqual(corr.index2(), "INDEX2")
        self.assertEqual(corr.expiry(), "1Y")
        self.assertEqual(corr.strike(), "0.1")

class CorrelationQuoteThrowTest(unittest.TestCase):

    def setUp(self):
        self.d = Date(3, March, 2018)
        self.value = 10
    
    def testMarketDatumParsingFail(self):
        with self.assertRaises(RuntimeError):
            parseMarketDatum(self.d, "CORRELATION/PRICE/INDEX1/INDEX2/1Y/SS", self.value)
            parseMarketDatum(self.d, "CORRELATION/PRICE/INDEX1/INDEX2/6X/0.1", self.value)

        
if __name__ == '__main__':
    import ORE
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(CorrelationRateQuoteTest,'test'))
    suite.addTest(unittest.makeSuite(CorrelationPriceQuoteTest,'test'))
    suite.addTest(unittest.makeSuite(CorrelationQuoteThrowTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

