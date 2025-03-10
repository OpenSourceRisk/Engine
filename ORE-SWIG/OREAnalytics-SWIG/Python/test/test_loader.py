
"""
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest
import os

class LoaderTest:

    def setUp(self):
        self.asofDate = Date(5, February, 2016)
        print(os.path.dirname(__file__))
        self.marketfile = os.path.join(os.path.dirname(__file__), "Input/market_20160205.txt")
        self.fixingfile = os.path.join(os.path.dirname(__file__), "Input/fixings_20160205.txt")
        marketdata = []
        fixingdata = []
        dominance = ["XAU", "XAG", "XPT", "XPD", "EUR", "GBP", "AUD", "NZD", "USD", "CAD", "CHF", "ZAR",
                    "MYR", "SGD", "DKK", "NOK", "SEK", "HKD", "THB", "TWD", "MXN", "CNY", "CNH",
                    "JPY", "IDR", "KRW"]
        import csv
        with open(self.marketfile, 'r') as csvfile:
            csv_reader = csv.reader(csvfile, delimiter=' ', quotechar='|')
            for row in csv_reader:
                if row is None or len(row) == 0 or row[0][0]== "#":
                    continue
                marketdatum = row[1].split('/')
                if marketdatum[0] == 'FX' and marketdatum[1] == 'RATE':
                    tmp = marketdatum[0] + '/' + marketdatum[1] + '/' + marketdatum[3] + '/' + marketdatum[2]
                    if any(d['Date'] == self.asofDate and d['Name'] == tmp for d in marketdata):
                        if (dominance.index(marketdatum[3]) < dominance.index(marketdatum[2])):
                            continue
                        elif (dominance.index(marketdatum[3]) > dominance.index(marketdatum[2])):
                            marketdata[:] = [x for x in marketdata if x.get('Name') != tmp]
                marketdata.append({
                    'Date': self.asofDate,
                    'Name': row[1],
                    'Value': float(row[-1])
                })
        with open(self.fixingfile, 'r') as csvfile:
            csv_reader = csv.reader(csvfile, delimiter=' ', quotechar='|')
            for row in csv_reader:
                if row is None or len(row) == 0 or row[0][0]== "#":
                    continue
                fixingdata.append({
                    'Date': parseDate(row[0]),
                    'Name': row[1],
                    'Value': float(row[-1])
                })
        self.marketdata = marketdata
        self.fixingdata = fixingdata
                
    def testSimpleInspectors(self):
        """ Test Loader simple inspectors. """
        self.assertEqual(len(self.marketdata_loader), len(self.marketdata))
        # fixing data loader returns a set now, not  a vector any more (with duplicates), hence skip the following assert
        # cat Input/fixings_20160205.txt| grep -v "#"| wc -l => 8691 in fixingdata
        # cat Input/fixings_20160205.txt| grep -v "#"| cut -f1,2 -d" "|sort -u| wc -l => 5435 in fixingdata_loader
        #self.assertEqual(len(self.fixingdata_loader), len(self.fixingdata))

        # market data is internally held in the loader in a map of sets, and due to the set it is ordered differently
        # to the market data vector. The following tests will therefore fail after the loader's redesign.
        #for i in range(len(self.marketdata)):
        #    self.assertEqual(self.marketdata_loader[i].asofDate(), self.marketdata[i]['Date'])
        #    self.assertEqual(self.marketdata_loader[i].name(), self.marketdata[i]['Name'])
        #    self.assertAlmostEqual(self.marketdata_loader[i].quote().value(), self.marketdata[i]['Value'])

        for i in range(len(self.marketdata)):
            self.assertTrue(self.loader.has(self.marketdata[i]['Name'], self.marketdata[i]['Date']))
            quote = self.loader.get(self.marketdata[i]['Name'], self.marketdata[i]['Date'])
            self.assertEqual(quote.asofDate(), self.marketdata[i]['Date'])
            self.assertEqual(quote.name(), self.marketdata[i]['Name'])
            self.assertAlmostEqual(quote.quote().value(), self.marketdata[i]['Value'])

        for i in range(len(self.marketdata)):
            quote = self.loader.get(StringBoolPair(self.marketdata[i]['Name'], True), self.marketdata[i]['Date'])
            self.assertEqual(quote.asofDate(), self.marketdata[i]['Date'])
            self.assertEqual(quote.name(), self.marketdata[i]['Name'])
            self.assertAlmostEqual(quote.quote().value(), self.marketdata[i]['Value'])

        for i in range(len(self.marketdata)):
            quote = self.loader.get(StringBoolPair(self.marketdata[i]['Name'], False), self.marketdata[i]['Date'])
            self.assertEqual(quote.asofDate(), self.marketdata[i]['Date'])
            self.assertEqual(quote.name(), self.marketdata[i]['Name'])
            self.assertAlmostEqual(quote.quote().value(), self.marketdata[i]['Value'])

# Comparison between fixing data (vector) to ficingdata_loader (set) fails because of different ordering.
# since the loader has been redesigned to hold data in sets rather than vectors
#        for i in range(len(self.fixingdata_loader)):
#            self.assertEqual(self.fixingdata_loader[i].date, self.fixingdata[i]['Date'])
#            self.assertEqual(self.fixingdata_loader[i].name, self.fixingdata[i]['Name'])
#            self.assertAlmostEqual(self.fixingdata_loader[i].fixing, self.fixingdata[i]['Value'])


class CSVLoaderTest(LoaderTest, unittest.TestCase):

    def setUp(self):
        """ Set-up CSV Loader """
        super(CSVLoaderTest, self).setUp()
        self.loader = CSVLoader(self.marketfile, self.fixingfile, True)
        self.marketdata_loader = self.loader.loadQuotes(self.asofDate)
        self.fixingdata_loader = self.loader.loadFixings()

    def testSimpleInspectors(self):
        """ Test CSV Loader simple inspectors. """
        super(CSVLoaderTest, self).testSimpleInspectors()

            
    def testConsistency(self):
        """ Test consistency of CSV Loader"""
        pass


class InMemoryLoaderTest(LoaderTest, unittest.TestCase):

    def setUp(self):
        """ Set-up InMemory Loader """
        super(InMemoryLoaderTest, self).setUp()
        self.loader = InMemoryLoader()
        for data in self.marketdata:
            self.loader.add(data['Date'], data['Name'], data['Value'])
        for data in self.fixingdata:
            self.loader.addFixing(data['Date'], data['Name'], data['Value'])
        self.marketdata_loader = self.loader.loadQuotes(self.asofDate)
        print("market data/loader size ", len(self.marketdata_loader), len(self.marketdata));
        self.fixingdata_loader = self.loader.loadFixings()

    def testSimpleInspectors(self):
        """ Test InMemory Loader simple inspectors. """
        super(InMemoryLoaderTest, self).testSimpleInspectors()
        
    def testConsistency(self):
        """ Test consistency of InMemory Loader"""
        pass


        
if __name__ == '__main__':
    import ORE
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(CSVLoaderTest,'test'))
    suite.addTest(unittest.makeSuite(InMemoryLoaderTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

