"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest


if __name__ == '__main__':
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(ImmFraRateHelperTest, 'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)

class AverageOISRateHelpersTest(unittest.TestCase):
    def setUp(self):
        """ Test consistency of Average OIS Rate Helpers"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)
        self.fixedRate = QuoteHandle(SimpleQuote(0.05))
        self.spotLagTenor = Period(1,Days)
        self.swapTenor = Period(10,Years)
        self.fixedTenor = Period(3,Months)
        self.fixedDayCounter=Actual360()
        self.fixedCalendar=UnitedStates(UnitedStates.NYSE)
        self.fixedConvention=Following
        self.fixedPaymentAdjustment=Following
        self.overnightIndex=Eonia()
        self.onTenor=Period(3,Months)
        self.onSpread=QuoteHandle(SimpleQuote(0.04))
        self.rateCutoff=1
        self.flat_forward=FlatForward(self.todays_date, 0.03, self.fixedDayCounter)
        self.termStructureOIS=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.onIndexGiven=True
        self.averageOisRateHelper=AverageOISRateHelper(self.fixedRate,self.spotLagTenor,self.swapTenor,self.fixedTenor,
                                                       self.fixedDayCounter,self.fixedCalendar,self.fixedConvention,
                                                       self.fixedPaymentAdjustment,self.overnightIndex, self.onIndexGiven, 
                                                       self.onTenor, self.onSpread,self.rateCutoff,self.termStructureOIS)

        
    def testSimpleInspectors(self):
        """ Test Avegare OIS Rate helpers simple inspector. """
        self.assertEqual(self.fixedRate.value(),self.averageOisRateHelper.quote().value())

class CrossCcyBasisSwapHelperTest(unittest.TestCase):     
    def setUp(self):
        """ Test consistency of Cross Curency Basis Swap Helper"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)   
        self.fixedDayCounter=Actual360()
        self.spreadQuote=QuoteHandle(SimpleQuote(0.05))
        self.spotFX=QuoteHandle(SimpleQuote(1.0))
        self.settlementDays=2
        self.settlementCalendar=UnitedStates(UnitedStates.NYSE)
        self.swapTenor=Period(3,Months)
        self.rollConvention=Following 
        self.forecast_curve = RelinkableYieldTermStructureHandle()
        self.forecast_curve.linkTo(FlatForward(self.todays_date, 0.03, self.fixedDayCounter,Compounded, Semiannual))
        self.forecast_curve2 = RelinkableYieldTermStructureHandle()
        self.flatIbor = Euribor6M(self.forecast_curve)
        self.spreadIbor = Euribor6M(self.forecast_curve2)
        self.flat_forward=FlatForward(self.todays_date, 0.03, self.fixedDayCounter)
        self.flatDiscountCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.spreadDiscountCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.flatIndexGiven=True
        self.spreadIndexGiven=True
        self.flatDiscountCurveGiven=True
        self.spreadDiscountCurveGiven=False
        self.eom=False
        self.flatIsDomestic=True
        self.ratehelper = CrossCcyBasisSwapHelper(self.spreadQuote, self.spotFX, self.settlementDays,
                                                  self.settlementCalendar, self.swapTenor, self.rollConvention, 
                                                  self.flatIbor, self.spreadIbor, self.flatDiscountCurve, 
                                                  self.spreadDiscountCurve, self.flatIndexGiven, self.spreadIndexGiven,
                                                  self.flatDiscountCurveGiven, self.spreadDiscountCurveGiven, 
                                                  self.eom, self.flatIsDomestic)

    def testSimpleInspectors(self):
        """ Test Cross Curency Basis Swap Helper simple inspector. """
        self.assertEqual(self.ratehelper.quote().value(),self.spreadQuote.value())
        

        
class TenorBasisSwapHelperTest(unittest.TestCase):     
    def setUp(self):
        """ Test consistency of Tenor Basis Swap Helper"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)   
        self.spread=QuoteHandle(SimpleQuote(0.02))
        self.swapTenor=Period(3,Months)
        self.forecast_curve = RelinkableYieldTermStructureHandle()
        self.longIbor = Euribor6M(self.forecast_curve)
        self.forecast_curve2 = RelinkableYieldTermStructureHandle()
        self.forecast_curve2.linkTo(FlatForward(self.todays_date, 0.04, Actual360(),Compounded, Semiannual))
        self.shortIbor = Euribor6M(self.forecast_curve2)
        self.shortPayTenor=Period(6,Months)
        self.longPayTenor=Period(6,Months)
        self.fixedDayCounter=Actual360()
        self.flat_forward=FlatForward(self.todays_date, 0.03, self.fixedDayCounter)
        self.discountingCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.payIndexGiven=True
        self.receiveIndexGiven=True
        self.discountingCurveGiven=False
        self.spreadOnShort = True
        self.includeSpread = False
        self.telescopicValueDates = False
        self.type = SubPeriodsCoupon1.Compounding
        self.tenorbasisswaphelper=self.tenorbasisswaphelper=TenorBasisSwapHelper(self.spread,self.swapTenor,self.longIbor,
                                                                                 self.shortIbor,self.discountingCurve,
                                                                                 self.payIndexGiven, self.receiveIndexGiven,
                                                                                 self.discountingCurveGiven,
                                                                                 self.spreadOnShort,self.includeSpread,
                                                                                 self.longPayTenor,self.shortPayTenor,
                                                                                 self.telescopicValueDates,self.type)
        

    def testSimpleInspectors(self):
        """ Test Tenor Basis Swap Helper simple inspector. """
        self.assertEqual(self.spread.value(),self.tenorbasisswaphelper.quote().value())

            
class SubPeriodsSwapHelperTest(unittest.TestCase):     
    def setUp(self):
        """ Test consistency of SubPeriods Basis Swap Helper"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)   
        self.spread=QuoteHandle(SimpleQuote(0.02))
        self.swapTenor=Period(6,Months)
        self.fixedTenor=Period(6,Months)
        self.fixedCalendar=UnitedStates(UnitedStates.NYSE)
        self.fixedDayCount=Actual360()
        self.fixedConvention=Following
        self.floatPayTenor=Period(6,Months)
        self.floatIndex=Eonia()
        self.floatDayCount=Actual360()
        self.flat_forward=FlatForward(self.todays_date, 0.03, self.floatDayCount)
        self.discountingCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.type=SubPeriodsCoupon1.Compounding
        self.subperiodsswaphelper=SubPeriodsSwapHelper(self.spread,self.swapTenor,self.fixedTenor,self.fixedCalendar,self.fixedDayCount,self.fixedConvention,self.floatPayTenor,self.floatIndex,self.floatDayCount,self.discountingCurve,self.type)
        
    def testSimpleInspectors(self):
        """ Test SubPeriods Basis Swap Helper simple inspector. """
        self.assertEqual(self.subperiodsswaphelper.quote().value(),self.spread.value())

class BasisTwoSwapHelperTest(unittest.TestCase):
    def setUp(self):
        """ Test consistency of basis to swap Helper"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)
        self.spread=QuoteHandle(SimpleQuote(0.02))
        self.swapTenor=Period(6,Months)
        self.calendar=UnitedStates(UnitedStates.NYSE)
        self.longFixedFrequency=Annual
        self.longFixedConvention=Following
        self.longFixedDayCount=Actual360()
        self.longFloat=Eonia()
        self.longIndexGiven=True
        self.shortFixedFrequency=Annual
        self.shortFixedConvention=Following
        self.shortFixedDayCount=Actual360()
        self.flat_forward=FlatForward(self.todays_date, 0.03, Actual360())
        self.ffcurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.shortFloat=FedFunds(self.ffcurve)
        self.longMinusShort=True
        self.shortIndexGiven=True
        self.discountingCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.discountCurveGiven=False
        self.basistwoswaphelper=BasisTwoSwapHelper(self.spread,self.swapTenor,self.calendar,
                                                   self.longFixedFrequency,self.longFixedConvention,
                                                   self.longFixedDayCount,self.longFloat,
                                                   self.longIndexGiven, self.shortFixedFrequency,
                                                   self.shortFixedConvention,self.shortFixedDayCount,
                                                   self.shortFloat,self.longMinusShort,self.shortIndexGiven,
                                                   self.discountingCurve, self.discountCurveGiven)


    def testSimpleInspectors(self):
        """ Test basis to swap Helper simple inspector. """
        self.assertEqual(self.basistwoswaphelper.quote().value(),self.spread.value())


class OICCBSHelperTest(unittest.TestCase):
    def setUp(self):
        """ Test OICCBS Helper"""
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)
        self.settlementDays=2
        self.term=Period(6,Months)
        self.payFloat=Sonia()
        self.payTenor=Period(6,Months)
        self.recFloat=Sonia()
        self.recTenor=Period(6,Months)
        self.spreadQuote=QuoteHandle(SimpleQuote(0.02))
        self.flat_forward=FlatForward(self.todays_date, 0.03, Actual360())
        self.fixedDiscountCurve=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.spreadQuoteOnPayLeg=True
        self.fixedDiscountOnPayLeg=False
        self.oiccbshelper=OICCBSHelper(self.settlementDays,self.term,self.payFloat,self.payTenor,self.recFloat,self.recTenor,self.spreadQuote,self.fixedDiscountCurve,self.spreadQuoteOnPayLeg,self.fixedDiscountOnPayLeg)        
    def testSimpleInspectors(self):
        """ Test OICCBS simple inspector. """
        self.assertEqual(self.oiccbshelper.quote().value(),self.spreadQuote.value())


class ImmFraRateHelperTest(unittest.TestCase):
    def setUp(self):
        """ Set-up ImmFraRateHelper """
        self.todays_date = Date(11, November, 2018)
        self.rate = QuoteHandle(SimpleQuote(0.02))
        self.day_counter = Actual365Fixed()
        self.size1 = 10
        self.size2 = 12
        self.flat_forward = FlatForward(self.todays_date, 0.005, self.day_counter)
        self.term_structure = RelinkableYieldTermStructureHandle(self.flat_forward)
        self.index = USDLibor(Period(3, Months), self.term_structure)
        self.immfraratehelper = ImmFraRateHelper(self.rate,self.size1,self.size2,self.index)
    
    def testSimpleInspectors(self):
        """ Test  ImmFraRateHelper simple inspector """
        self.assertEqual(self.immfraratehelper.quote().value(),self.rate.value())

class CrossCcyFixFloatSwapHelperTest(unittest.TestCase):
    def setUp(self):
        """ Set-up crossccyfixfloat rate helper """
        self.todays_date=Date(1,October,2018)
        Settings.instance().setEvaluationDate(self.todays_date)
        self.rate=QuoteHandle(SimpleQuote(0.02))
        self.spotFx=QuoteHandle(SimpleQuote(1.0))
        self.settlementDays=2
        self.paymentCalendar=UnitedStates(UnitedStates.NYSE)
        self.paymentConvention=Following
        self.tenor=Period(6,Months)
        self.fixedCurrency=USDCurrency()
        self.fixedFrequency=Annual
        self.fixedConvention=Following
        self.fixedDayCount=Actual360()
        self.indexSwap=Eonia()
        self.flat_forward=FlatForward(self.todays_date, 0.03, Actual360())
        self.floatDiscount=RelinkableYieldTermStructureHandle(self.flat_forward)
        self.spread=QuoteHandle(SimpleQuote(0.03))
        self.crossccyfixfloatswaphelper=CrossCcyFixFloatSwapHelper(self.rate,self.spotFx,self.settlementDays,self.paymentCalendar,self.paymentConvention,self.tenor,self.fixedCurrency,self.fixedFrequency,self.fixedConvention,self.fixedDayCount,self.indexSwap,self.floatDiscount,self.spread)

    def testSimpleInspectors(self):
        """ Test crossccyfixfloat rate helper simple inspector """
        self.assertEqual(self.crossccyfixfloatswaphelper.quote().value(),self.rate.value())

    
if __name__ == '__main__':
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(AverageOISRateHelpersTest,'test'))
    suite.addTest(unittest.makeSuite(CrossCcyBasisSwapHelperTest,'test'))
    suite.addTest(unittest.makeSuite(TenorBasisSwapHelperTest,'test'))
    suite.addTest(unittest.makeSuite(SubPeriodsSwapHelperTest,'test'))
    suite.addTest(unittest.makeSuite(BasisTwoSwapHelperTest,'test'))
    suite.addTest(unittest.makeSuite(OICCBSHelperTest,'test'))
    suite.addTest(unittest.makeSuite(ImmFraRateHelperTest,'test'))
    suite.addTest(unittest.makeSuite(CrossCcyFixFloatSwapHelperTest,'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)
    unittest.main()

