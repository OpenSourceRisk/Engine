"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest

class CrossCcyBasisSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up CrossCcyBasisSwap """
        self.todays_date = Date(11, November, 2018)
        self.pay_nominal = 10000000
        self.rec_nominal = 12000000
        self.pay_currency = USDCurrency()
        self.rec_currency = EURCurrency()
        self.settlement_date = Date(13, November, 2018)
        self.calendar = TARGET()
        self.day_counter = Actual365Fixed()
        self.forward_start = self.calendar.advance(self.settlement_date, 1, Years)
        self.forward_end = self.calendar.advance(self.forward_start, 5, Years)
        self.tenor = Period(6, Months)
        self.bdc = ModifiedFollowing
        self.schedule = Schedule(self.forward_start, self.forward_end, self.tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.flat_forward_USD = FlatForward(self.todays_date, 0.005, self.day_counter)
        self.discount_term_structure_USD = RelinkableYieldTermStructureHandle(self.flat_forward_USD)
        self.flat_forward_EUR = FlatForward(self.todays_date, 0.03, self.day_counter)
        self.discount_term_structure_EUR = RelinkableYieldTermStructureHandle(self.flat_forward_EUR)
        self.pay_spread = 0.0
        self.rec_spread = 0.0
        self.pay_gearing = 1.0
        self.rec_gearing = 1.0
        self.pay_index = USDLibor(Period(3, Months), self.discount_term_structure_USD)
        self.rec_index = Euribor3M(self.discount_term_structure_EUR)
        self.fxQuote = QuoteHandle(SimpleQuote(1/0.8))
        self.swap = CrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                      self.pay_index, self.pay_spread, self.pay_gearing,
                                      self.rec_nominal, self.rec_currency, self.schedule,
                                      self.rec_index, self.rec_spread, self.rec_gearing)
        self.engine = CrossCcySwapEngine(self.pay_currency, self.discount_term_structure_USD,
                                         self.rec_currency, self.discount_term_structure_EUR,
                                         self.fxQuote, False, self.settlement_date, self.todays_date)
        self.swap.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test CrossCcyBasisSwap simple inspectors. """
        self.assertEqual(self.swap.payNominal(), self.pay_nominal)
        self.assertEqual(self.swap.payCurrency(), self.pay_currency)
        self.assertEqual(self.swap.paySpread(), self.pay_spread)
        self.assertEqual(self.swap.recNominal(), self.rec_nominal)
        self.assertEqual(self.swap.recCurrency(), self.rec_currency)
        self.assertEqual(self.swap.recSpread(), self.rec_spread)
        
    def testSchedules(self):
        """ Test CrossCcyBasisSwap schedules. """
        for i, d in enumerate(self.schedule):
            self.assertEqual(self.swap.paySchedule()[i], d)
            self.assertEqual(self.swap.recSchedule()[i], d)
            
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        fair_rec_spread = self.swap.fairPaySpread()
        swap = CrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                 self.pay_index, self.pay_spread, self.pay_gearing,
                                 self.rec_nominal, self.rec_currency, self.schedule,
                                 self.rec_index, fair_rec_spread, self.rec_gearing)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)
        fair_pay_spread = self.swap.fairPaySpread()
        swap = CrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                 self.pay_index, fair_pay_spread, self.pay_gearing,
                                 self.rec_nominal, self.rec_currency, self.schedule,
                                 self.rec_index, self.rec_spread, self.rec_gearing)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)

class CDSOptionTest(unittest.TestCase):
    def setUp(self):
        """ Set-up CDSOptionTest and engine"""
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.pay_tenor = Period(Quarterly)
        self.calendar = TARGET()
        self.settlement_date = self.calendar.advance(self.todays_date, Period(2, Years))
        self.maturity_date = self.calendar.advance(self.todays_date, Period(5, Years))
        self.exercise_date = self.calendar.advance(self.todays_date, Period(1, Years))
        self.exercise = EuropeanExercise(self.exercise_date)
        self.bdc = ModifiedFollowing
        self.day_counter = Actual365Fixed()
        self.notional = 10000000
        self.schedule = Schedule(self.settlement_date, self.maturity_date,
                                 self.pay_tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.spread = 0.01
        self.side = Protection.Buyer
        self.upfront = 0
        self.recovery_rate = 0.4
        self.settles_accrual = True
        self.protection_payment_time = True
        self.discount_curve = YieldTermStructureHandle(FlatForward(self.todays_date, 0.01, self.day_counter))
        self.hazard_curve = FlatHazardRate(self.todays_date, QuoteHandle(SimpleQuote(0.02)), self.day_counter)
        self.probability_curve = DefaultProbabilityTermStructureHandle(self.hazard_curve)
        self.vol = 0.03
        self.volatility = BlackConstantVol(0, self.calendar, QuoteHandle(SimpleQuote(self.vol)), self.day_counter)
        self.volatility_curve = BlackVolTermStructureHandle(self.volatility)
        self.creditVolWrap = CreditVolCurveWrapper(self.volatility_curve)
        self.creditVolHandle = RelinkableVolCreditCurveHandle(self.creditVolWrap)
        self.cds = CreditDefaultSwap(self.side, self.notional, self.upfront, self.spread,
                                        self.schedule, self.bdc, self.day_counter,
                                        self.settles_accrual, self.protection_payment_time,
                                        self.settlement_date)
        self.cds_engine = MidPointCdsEngine(self.probability_curve, self.recovery_rate, self.discount_curve)
        self.cds.setPricingEngine(self.cds_engine)
        self.cds_option = QLECdsOption(self.cds, self.exercise)
        self.engine = QLEBlackCdsOptionEngine(self.probability_curve, self.recovery_rate,
                                              self.discount_curve, self.creditVolHandle)
        self.cds_option.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test CDSOptionTest simple inspectors. """
        self.assertEqual(self.cds_option.underlyingSwap().notional(), self.cds.notional())
        self.assertEqual(self.cds_option.underlyingSwap().side(), self.cds.side())
        
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        atm_rate = self.cds_option.atmRate()
        buyer_cds = CreditDefaultSwap(Protection.Buyer, self.notional, self.upfront, atm_rate,
                                         self.schedule, self.bdc, self.day_counter,
                                         self.settles_accrual, self.protection_payment_time,
                                         self.settlement_date)
        seller_cds = CreditDefaultSwap(Protection.Seller, self.notional, self.upfront, atm_rate,
                                          self.schedule, self.bdc, self.day_counter,
                                          self.settles_accrual, self.protection_payment_time,
                                          self.settlement_date)
        buyer_cds.setPricingEngine(self.cds_engine)
        seller_cds.setPricingEngine(self.cds_engine)
        buyer_cds_option = QLECdsOption(buyer_cds, self.exercise)
        seller_cds_option = QLECdsOption(seller_cds, self.exercise)
        buyer_cds_option.setPricingEngine(self.engine)
        seller_cds_option.setPricingEngine(self.engine)
        self.assertFalse(abs(buyer_cds_option.NPV() - seller_cds_option.NPV()) > tolerance)
        implied_vol = self.cds_option.impliedVolatility(self.cds_option.NPV(), self.discount_curve,
                                                        self.probability_curve, self.recovery_rate)
        self.assertFalse(abs(implied_vol - self.vol) > 1.0e-5)
        
        
class CreditDefaultSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up CreditDefaultSwap and engine"""
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.settlement_date = Date(6, October, 2018)
        self.swap_tenor = Period(2, Years)
        self.pay_tenor = Period(Quarterly)
        self.calendar = TARGET()
        self.maturity_date = self.calendar.advance(self.settlement_date, self.swap_tenor)
        self.bdc = ModifiedFollowing
        self.day_counter = Actual365Fixed()
        self.notional = 10000000
        self.schedule = Schedule(self.settlement_date, self.maturity_date,
                                 self.pay_tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.spread = 0.012
        self.side = Protection.Buyer
        self.upfront = 0
        self.settles_accrual = True
        self.protection_payment_time = True
        self.discount_curve = YieldTermStructureHandle(FlatForward(self.todays_date, 0.01, self.day_counter))
        self.recovery_rate = 0.4
        self.hazard_rate = 0.015
        self.hazard_curve = FlatHazardRate(self.todays_date, QuoteHandle(SimpleQuote(self.hazard_rate)), self.day_counter)
        self.probability_curve = DefaultProbabilityTermStructureHandle(self.hazard_curve)
        self.cds = CreditDefaultSwap(self.side, self.notional, self.upfront, self.spread,
                                        self.schedule, self.bdc, self.day_counter,
                                        self.settles_accrual, self.protection_payment_time,
                                        self.settlement_date)
        self.engine = MidPointCdsEngine(self.probability_curve, self.recovery_rate, self.discount_curve)
        self.cds.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test CreditDefaultSwap simple inspectors. """
        self.assertEqual(self.cds.side(), self.side)
        self.assertEqual(self.cds.notional(), self.notional)
        self.assertEqual(self.cds.runningSpread(), self.spread)
        self.assertEqual(self.cds.settlesAccrual(), self.settles_accrual)
        self.assertEqual(self.cds.paysAtDefaultTime(), self.protection_payment_time)
        self.assertEqual(self.cds.protectionStartDate(), self.settlement_date)
        self.assertEqual(self.cds.protectionEndDate(), self.maturity_date)
        
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        fair_spread = self.cds.fairSpread()
        cds = CreditDefaultSwap(self.side, self.notional, self.upfront, fair_spread,
                                   self.schedule, self.bdc, self.day_counter,
                                   self.settles_accrual, self.protection_payment_time,
                                   self.settlement_date)
        cds.setPricingEngine(self.engine)
        self.assertFalse(abs(cds.NPV()) > tolerance)
        implied_hazard_rate = self.cds.impliedHazardRate(self.cds.NPV(), self.discount_curve, self.day_counter,
                                                         self.recovery_rate, 1.0e-12)
        self.assertFalse(abs(implied_hazard_rate - self.hazard_rate) > tolerance)
         

class OvernightIndexedCrossCcyBasisSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up OvernightIndexedCrossCcyBasisSwap and engine"""
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.settlement_date = Date(6, October, 2018)
        self.swap_tenor = Period(10, Years)
        self.pay_tenor = Period(3, Months)
        self.calendar = UnitedStates(UnitedStates.NYSE)
        self.pay_currency = USDCurrency()
        self.rec_currency = EURCurrency()
        self.maturity_date = self.calendar.advance(self.settlement_date, self.swap_tenor)
        self.bdc = ModifiedFollowing
        self.day_counter = Actual365Fixed()
        self.pay_nominal = 10000000
        self.rec_nominal = 10000000
        self.schedule = Schedule(self.settlement_date, self.maturity_date,
                                 self.pay_tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.pay_spread = 0.005
        self.rec_spread = 0.0
        self.fxQuote = QuoteHandle(SimpleQuote(1.1))
        self.USD_OIS_flat_forward = FlatForward(self.todays_date, 0.01, self.day_counter)
        self.USD_OIS_term_structure = RelinkableYieldTermStructureHandle(self.USD_OIS_flat_forward)
        self.USD_OIS_index = FedFunds(self.USD_OIS_term_structure)
        self.EUR_OIS_flat_forward = FlatForward(self.todays_date, 0.01, self.day_counter)
        self.EUR_OIS_term_structure = RelinkableYieldTermStructureHandle(self.EUR_OIS_flat_forward)
        self.EUR_OIS_index = Eonia(self.EUR_OIS_term_structure)
        self.swap = OvernightIndexedCrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                                      self.USD_OIS_index, self.pay_spread, self.rec_nominal, 
                                                      self.rec_currency, self.schedule, self.EUR_OIS_index, 
                                                      self.rec_spread)
        self.engine = OvernightIndexedCrossCcyBasisSwapEngine(self.USD_OIS_term_structure, self.pay_currency,
                                                              self.EUR_OIS_term_structure, self.rec_currency,
                                                              self.fxQuote)
        self.swap.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test OvernightIndexedCrossCcyBasisSwap and engine simple inspectors. """
        self.assertEqual(self.swap.payNominal(), self.pay_nominal)
        self.assertEqual(self.swap.recNominal(), self.rec_nominal)
        self.assertEqual(self.swap.payCurrency(), self.pay_currency)
        self.assertEqual(self.swap.recCurrency(), self.rec_currency)
        self.assertEqual(self.swap.paySpread(), self.pay_spread)
        self.assertEqual(self.swap.recSpread(), self.rec_spread)
        self.assertEqual(self.engine.ccy1(), self.pay_currency)
        self.assertEqual(self.engine.ccy2(), self.rec_currency)

    def testSchedules(self):
        """ Test OvernightIndexedCrossCcyBasisSwap schedules. """
        for i, d in enumerate(self.schedule):
            self.assertEqual(self.swap.recSchedule()[i], d)
            self.assertEqual(self.swap.paySchedule()[i], d)
            
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        fair_pay_spread = self.swap.fairPayLegSpread()
        swap = OvernightIndexedCrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                                 self.USD_OIS_index, fair_pay_spread, self.rec_nominal, 
                                                 self.rec_currency, self.schedule, self.EUR_OIS_index, 
                                                 self.rec_spread)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)
        fair_rec_spread = self.swap.fairRecLegSpread()
        swap = OvernightIndexedCrossCcyBasisSwap(self.pay_nominal, self.pay_currency, self.schedule,
                                                 self.USD_OIS_index, self.pay_spread, self.rec_nominal, 
                                                 self.rec_currency, self.schedule, self.EUR_OIS_index, 
                                                 fair_rec_spread)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)
class AverageOISTest(unittest.TestCase):
    def setUp(self):
        """ Set-up AverageOIS and engine """
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.settlement_date = Date(6, October, 2018)
        self.swap_tenor = Period(10, Years)
        self.pay_tenor = Period(6, Months)
        self.calendar = UnitedStates(UnitedStates.NYSE)
        self.maturity_date = self.calendar.advance(self.settlement_date, self.swap_tenor)
        self.type = AverageOIS.Payer
        self.bdc = ModifiedFollowing
        self.day_counter = Actual365Fixed()
        self.nominal = 10000000
        self.schedule = Schedule(self.settlement_date, self.maturity_date,
                                 self.pay_tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.fixed_rate = 0.03
        self.flat_forward = FlatForward(self.todays_date, 0.02, self.day_counter)
        self.term_structure = RelinkableYieldTermStructureHandle(self.flat_forward)
        self.index = FedFunds(self.term_structure)
        self.rate_cut_off = 0
        self.on_spread = 0.0
        self.on_gearing = 1.0
        self.coupon_pricer = AverageONIndexedCouponPricer(AverageONIndexedCouponPricer.Takada)
        self.swap = AverageOIS(self.type, self.nominal, self.schedule, self.fixed_rate,
                               self.day_counter, self.bdc, self.calendar, self.schedule,
                               self.index, self.bdc, self.calendar, self.rate_cut_off, 
                               self.on_spread, self.on_gearing, self.day_counter, self.coupon_pricer)
        self.engine = DiscountingSwapEngine(self.term_structure)
        self.swap.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test AverageOIS simple inspectors. """
        self.assertEqual(self.swap.type(), self.type)
        self.assertEqual(self.swap.nominal(), self.nominal)
        self.assertEqual(self.swap.fixedRate(), self.fixed_rate)
        self.assertEqual(self.swap.fixedDayCounter(), self.day_counter)
        self.assertEqual(self.swap.rateCutoff(), self.rate_cut_off)
        self.assertEqual(self.swap.onSpread(), self.on_spread)
        self.assertEqual(self.swap.onGearing(), self.on_gearing)
        self.assertEqual(self.swap.onDayCounter(), self.day_counter)
        self.assertEqual(self.swap.overnightIndex().name(), self.index.name())
        self.assertEqual(self.swap.overnightIndex().businessDayConvention(), self.index.businessDayConvention())
            
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        fair_fixed_rate = self.swap.fairRate()
        swap = AverageOIS(self.type, self.nominal, self.schedule, fair_fixed_rate,
                          self.day_counter, self.bdc, self.calendar, self.schedule,
                          self.index, self.bdc, self.calendar, self.rate_cut_off, 
                          self.on_spread, self.on_gearing, self.day_counter, self.coupon_pricer)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)
        fair_spread = self.swap.fairSpread()
        swap = AverageOIS(self.type, self.nominal, self.schedule, self.fixed_rate,
                          self.day_counter, self.bdc, self.calendar, self.schedule,
                          self.index, self.bdc, self.calendar, self.rate_cut_off, 
                          fair_spread, self.on_gearing, self.day_counter, self.coupon_pricer)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)

class CrossCurrencyFixFloatSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up CrossCurrencyFixFloatSwap and engine """
        self.todayDate = Date(11, November, 2018)
        Settings.instance().evaluationDate = self.todayDate
        self.nominal1 = 10000000
        self.nominal2 = 12000000
        self.currency1 = USDCurrency()
        self.currency2 = EURCurrency()
        self.settlementDate = Date(13, November, 2018)
        self.calendar = TARGET()
        self.tsDayCounter = Actual365Fixed()
        self.busDayConvention = Following
        self.forwardStart = self.calendar.advance(self.settlementDate, 1, Years)
        self.forwardEnd = self.calendar.advance(self.forwardStart, 5, Years)
        self.payLag = 1
        self.tenor = Period(6, Months)
        self.bdc = ModifiedFollowing
        self.schedule = Schedule(self.forwardStart, self.forwardEnd, self.tenor, self.calendar,
                                 self.bdc, self.bdc, DateGeneration.Forward, False)
        self.flatForwardUSD = FlatForward(self.todayDate, 0.005, self.tsDayCounter)
        self.discountTermStructureUSD = RelinkableYieldTermStructureHandle(self.flatForwardUSD)
        self.flatForwardEUR = FlatForward(self.todayDate, 0.03, self.tsDayCounter)
        self.discountTermStructureEUR = RelinkableYieldTermStructureHandle(self.flatForwardEUR)
        self.floatSpread = 0.0
        self.index = USDLibor(Period(3, Months), self.discountTermStructureUSD)
        self.couponRate = 0.1
        self.type = CrossCcyFixFloatSwap.Payer
        self.fxQuote = QuoteHandle(SimpleQuote(1/0.8))
        self.swap = CrossCcyFixFloatSwap(self.type, self.nominal2, self.currency2,
                                         self.schedule, self.couponRate, self.tsDayCounter,
                                         self.busDayConvention, self.payLag, self.calendar,
                                         self.nominal1, self.currency1, self.schedule,
                                         self.index, self.floatSpread, self.busDayConvention,
                                         self.payLag, self.calendar)
        self.engine = CrossCcySwapEngine(self.currency1, self.discountTermStructureEUR, self.currency2,
                                         self.discountTermStructureUSD, self.fxQuote, False,
                                         self.settlementDate, self.todayDate)
        self.swap.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test CrossCurrencyFixFloatSwap simple inspectors. """
        self.assertEqual(self.swap.type(), self.type)
        self.assertEqual(self.swap.fixedNominal(), self.nominal2)
        self.assertEqual(self.swap.fixedCurrency(), self.currency2)
        self.assertEqual(self.swap.fixedRate(), self.couponRate)
        self.assertEqual(self.swap.fixedDayCount(), self.tsDayCounter)
        self.assertEqual(self.swap.fixedPaymentBdc(), self.busDayConvention)
        self.assertEqual(self.swap.fixedPaymentLag(), self.payLag)
        self.assertEqual(self.swap.fixedPaymentCalendar(), self.calendar)
        self.assertEqual(self.swap.floatNominal(), self.nominal1)
        self.assertEqual(self.swap.floatCurrency(), self.currency1)
        self.assertEqual(self.swap.floatSpread(), self.floatSpread)
        self.assertEqual(self.swap.floatPaymentBdc(), self.busDayConvention)
        self.assertEqual(self.swap.floatPaymentLag(), self.payLag)
        self.assertEqual(self.swap.floatPaymentCalendar(), self.calendar)
        
    def testSchedules(self):
        """ Test CrossCurrencyFixFloatSwap schedules. """
        for i, d in enumerate(self.schedule):
            self.assertEqual(self.swap.fixedSchedule()[i], d)
            self.assertEqual(self.swap.floatSchedule()[i], d)
            
    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-8
        npv = self.swap.NPV()
        fair_fixed_rate = self.swap.fairFixedRate()
        swap = CrossCcyFixFloatSwap(self.type, self.nominal2, self.currency2,
                                    self.schedule, fair_fixed_rate, self.tsDayCounter,
                                    self.busDayConvention, self.payLag, self.calendar,
                                    self.nominal1, self.currency1, self.schedule,
                                    self.index, self.floatSpread, self.busDayConvention,
                                    self.payLag, self.calendar)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)
        fair_spread = self.swap.fairSpread()
        swap = CrossCcyFixFloatSwap(self.type, self.nominal2, self.currency2,
                                    self.schedule, self.couponRate, self.tsDayCounter,
                                    self.busDayConvention, self.payLag, self.calendar,
                                    self.nominal1, self.currency1, self.schedule,
                                    self.index, fair_spread, self.busDayConvention,
                                    self.payLag, self.calendar)
        swap.setPricingEngine(self.engine)
        self.assertFalse(abs(swap.NPV()) > tolerance)

        
class CommodityForwardTest(unittest.TestCase):
    def setUp(self):
        """ Set-up CommodityForward and engine """
        self.todays_date = Date(4, October, 2022)
        Settings.instance().evaluationDate = self.todays_date
        self.name = "Natural Gas"
        self.calendar = TARGET()
        self.currency = GBPCurrency()
        self.strike = 100.0
        self.quantity = 1.0
        self.position = Position.Long
        self.maturity_date = Date(4, October, 2022)
        self.day_counter = Actual365Fixed()
        self.dates = [ Date(20,12,2018), Date(20, 3,2019), Date(19, 6,2019),
                       Date(18, 9,2019), Date(18,12,2019), Date(19, 3,2020),
                       Date(18, 6,2020), Date(17, 9,2020), Date(17,12,2020) ]
        self.quotes = [ QuoteHandle(SimpleQuote(100.0)), QuoteHandle(SimpleQuote(100.25)),
                        QuoteHandle(SimpleQuote(100.75)), QuoteHandle(SimpleQuote(101.0)),
                        QuoteHandle(SimpleQuote(101.25)), QuoteHandle(SimpleQuote(101.50)),
                        QuoteHandle(SimpleQuote(101.75)), QuoteHandle(SimpleQuote(102.0)),
                        QuoteHandle(SimpleQuote(102.25)) ]
        self.price_curve = LinearInterpolatedPriceCurve(self.todays_date, self.dates, self.quotes, self.day_counter, self.currency)
        self.price_curve.enableExtrapolation()
        self.price_term_structure = RelinkablePriceTermStructureHandle(self.price_curve)
        self.index = CommoditySpotIndex(self.name, self.calendar, self.price_term_structure)
        self.commodity_forward = CommodityForward(self.index, self.currency, 
                                                  self.position, self.quantity, 
                                                  self.maturity_date, self.strike)
        self.flat_forward = FlatForward(self.todays_date, 0.03, self.day_counter)
        self.discount_term_structure = RelinkableYieldTermStructureHandle(self.flat_forward)
        self.engine = DiscountingCommodityForwardEngine(self.discount_term_structure)
        self.commodity_forward.setPricingEngine(self.engine)
  
    def testSimpleInspectors(self):
        """ Test CommodityForward simple inspectors. """
        self.assertEqual(self.commodity_forward.index().name(), self.index.name())
        self.assertEqual(self.commodity_forward.currency(), self.currency)
        self.assertEqual(self.commodity_forward.position(), self.position)
        self.assertEqual(self.commodity_forward.quantity(), self.quantity)
        self.assertEqual(self.commodity_forward.maturityDate(), self.maturity_date)
        self.assertEqual(self.commodity_forward.strike(), self.strike)
        
    def testNPV(self):
        """ Test NPV() """
        self.assertIsNotNone(self.commodity_forward.NPV())
        

class SubPeriodsSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up SubPeriodsSwap. """
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.settlement_date = Date(6, October, 2018)
        self.is_payer = True
        self.fixed_rate = 0.02
        self.sub_periods_type = SubPeriodsCoupon1.Compounding
        self.calendar = TARGET()
        self.swap_tenor = Period(10, Years)
        self.maturity_date = self.calendar.advance(self.settlement_date, self.swap_tenor)
        self.float_pay_tenor = Period(6, Months)
        self.fixed_tenor = Period(6, Months)
        self.day_counter = Actual365Fixed()
        self.nominal = 1000000.0
        self.bdc = ModifiedFollowing
        self.date_generation = DateGeneration.Forward
        self.ois_term_structure = RelinkableYieldTermStructureHandle()
        self.float_term_structure = RelinkableYieldTermStructureHandle()
        self.float_index = Euribor3M(self.float_term_structure)
        self.schedule = Schedule(self.settlement_date, self.maturity_date,
                                 self.float_pay_tenor, self.calendar,
                                 self.bdc, self.bdc, self.date_generation, False)
        self.sub_periods_swap = SubPeriodsSwap(self.settlement_date, self.nominal, self.swap_tenor,
                                               self.is_payer, self.fixed_tenor, self.fixed_rate,
                                               self.calendar, self.day_counter, self.bdc, 
                                               self.float_pay_tenor, self.float_index, 
                                               self.day_counter, self.date_generation, self.sub_periods_type)
        self.float_flat_forward = FlatForward(self.todays_date, 0.03, self.day_counter)
        self.ois_flat_forward = FlatForward(self.todays_date, 0.01, self.day_counter)
        self.float_term_structure.linkTo(self.float_flat_forward)
        self.ois_term_structure.linkTo(self.ois_flat_forward)
        self.engine = DiscountingSwapEngine(self.ois_term_structure)
        self.sub_periods_swap.setPricingEngine(self.engine)
  
    def testSimpleInspectors(self):
        """ Test SubPeriodsSwap simple inspectors. """
        self.assertEqual(self.sub_periods_swap.nominal(), self.nominal)
        self.assertEqual(self.sub_periods_swap.isPayer(), self.is_payer)
        self.assertEqual(self.sub_periods_swap.fixedRate(), self.fixed_rate)
        self.assertEqual(self.sub_periods_swap.type(), self.sub_periods_type)
        self.assertEqual(self.sub_periods_swap.floatPayTenor(), self.float_pay_tenor)
    
    def testSchedules(self):
        """ Test SubPeriodsSwap schedules. """
        for i, d in enumerate(self.schedule):
            self.assertEqual(self.sub_periods_swap.fixedSchedule()[i], d)

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        # lower tolerance, since fair rate is approximated using PV01 for this class
        tolerance = 1.0e-8
        fair_rate = self.sub_periods_swap.fairRate()
        sub_periods_swap = SubPeriodsSwap(self.settlement_date, self.nominal, self.swap_tenor,
                                          self.is_payer, self.fixed_tenor, fair_rate,
                                          self.calendar, self.day_counter, self.bdc, 
                                          self.float_pay_tenor, self.float_index, 
                                          self.day_counter, self.date_generation, self.sub_periods_type)
        sub_periods_swap.setPricingEngine(self.engine)
        self.assertFalse(abs(sub_periods_swap.NPV()) > tolerance)


class TenorBasisSwapTest(unittest.TestCase):
    def setUp(self):
        """ Set-up TenorBasisSwap and engine. """
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.settlement_date = Date(6, October, 2018)
        self.calendar = TARGET()
        self.day_counter = Actual365Fixed()
        self.nominal = 1000000.0
        self.maturity_date = self.calendar.advance(self.settlement_date, 5, Years)
        self.receiveFrequency = Period(6, Months)
        self.payFrequency = Period(6, Months)
        self.receive_index_leg_spread = 0.00
        self.pay_index_leg_spread = 0.00
        self.bdc = ModifiedFollowing
        self.date_generation = DateGeneration.Forward
        self.end_of_month = False
        self.include_spread = False
        self.spreadOnRec = True
        self.sub_periods_type = SubPeriodsCoupon1.Compounding
        self.ois_term_structure = RelinkableYieldTermStructureHandle()
        self.rec_index_term_structure = RelinkableYieldTermStructureHandle()
        self.pay_index_term_structure = RelinkableYieldTermStructureHandle()
        self.rec_index = Euribor3M(self.rec_index_term_structure)
        self.pay_index = Euribor6M(self.pay_index_term_structure)
        self.telescopicValueDates = False
        self.rec_index_schedule = Schedule(self.settlement_date, self.maturity_date,
                                            self.receiveFrequency, self.calendar,
                                            self.bdc, self.bdc, self.date_generation,
                                            self.end_of_month)
        self.pay_index_schedule = Schedule(self.settlement_date, self.maturity_date,
                                            self.payFrequency, self.calendar,
                                            self.bdc, self.bdc, self.date_generation,
                                            self.end_of_month)
        self.tenor_basis_swap = TenorBasisSwap(self.nominal, self.pay_index_schedule,
                                               self.pay_index, self.pay_index_leg_spread,
                                               self.rec_index_schedule, self.rec_index,
                                               self.receive_index_leg_spread, self.include_spread,
                                               self.spreadOnRec, self.sub_periods_type, self.telescopicValueDates)
        self.rec_index_flat_forward = FlatForward(self.todays_date, 0.02, self.rec_index.dayCounter())
        self.pay_index_flat_forward = FlatForward(self.todays_date, 0.03, self.pay_index.dayCounter())
        self.ois_flat_forward = FlatForward(self.todays_date, 0.01, self.day_counter)
        self.rec_index_term_structure.linkTo(self.rec_index_flat_forward)
        self.pay_index_term_structure.linkTo(self.pay_index_flat_forward)
        self.ois_term_structure.linkTo(self.ois_flat_forward)
        self.engine = DiscountingSwapEngine(self.ois_term_structure)
        self.tenor_basis_swap.setPricingEngine(self.engine)
  
    def testSimpleInspectors(self):
        """ Test TenorBasisSwap simple inspectors. """
        self.assertEqual(self.tenor_basis_swap.nominal(), self.nominal)
        self.assertEqual(self.tenor_basis_swap.paySpread(), self.pay_index_leg_spread)
        self.assertEqual(self.tenor_basis_swap.recSpread(), self.receive_index_leg_spread)
        self.assertEqual(self.tenor_basis_swap.type(), self.sub_periods_type)
        self.assertEqual(self.tenor_basis_swap.includeSpread(), self.include_spread)
    
    def testSchedules(self):
        """ Test TenorBasisSwap schedules. """
        for i, d in enumerate(self.pay_index_schedule):
            self.assertEqual(self.tenor_basis_swap.paySchedule()[i], d)
        for i, d in enumerate(self.rec_index_schedule):
            self.assertEqual(self.tenor_basis_swap.recSchedule()[i], d)

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-10
        fair_rec_leg_spread = self.tenor_basis_swap.fairRecLegSpread()
        tenor_basis_swap = TenorBasisSwap(self.nominal, self.pay_index_schedule,
                                          self.pay_index, self.pay_index_leg_spread,
                                          self.rec_index_schedule, self.rec_index, 
                                          fair_rec_leg_spread, self.include_spread, 
                                          self.spreadOnRec, self.sub_periods_type,
                                          self.telescopicValueDates)
        tenor_basis_swap.setPricingEngine(self.engine)
        self.assertFalse(abs(tenor_basis_swap.NPV()) > tolerance)

class FxForwardTest(unittest.TestCase):
    def setUp(self):
        """ Set-up FxForward and engine. """
        self.todays_date = Date(4, October, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.nominal1 = 1000
        self.nominal2 = 1000
        self.currency1 = GBPCurrency()
        self.currency2 = EURCurrency()
        self.maturity_date = Date(4, October, 2022)
        self.pay_currency1 = True
        self.day_counter = Actual365Fixed()
        self.fxForward = FxForward(self.nominal1, self.currency1,
                                   self.nominal2, self.currency2,
                                   self.maturity_date, self.pay_currency1)
        self.spot_fx = QuoteHandle(SimpleQuote(1.1))
        self.gbp_flat_forward = FlatForward(self.todays_date, 0.03, self.day_counter)
        self.eur_flat_forward = FlatForward(self.todays_date, 0.03, self.day_counter)
        self.gbp_term_structure_handle = RelinkableYieldTermStructureHandle(self.gbp_flat_forward)
        self.eur_term_structure_handle = RelinkableYieldTermStructureHandle(self.eur_flat_forward)
        self.engine = DiscountingFxForwardEngine(self.currency1, self.gbp_term_structure_handle,
                                                 self.currency2, self.eur_term_structure_handle,
                                                 self.spot_fx)
        self.fxForward.setPricingEngine(self.engine)
  
    def testSimpleInspectors(self):
        """ Test FxForward simple inspectors. """
        self.assertEqual(self.fxForward.currency1Nominal(), self.nominal1)
        self.assertEqual(self.fxForward.currency2Nominal(), self.nominal2)
        self.assertEqual(self.fxForward.maturityDate(), self.maturity_date)
        self.assertEqual(self.fxForward.payCurrency1(), self.pay_currency1)
        self.assertEqual(self.fxForward.currency1(), self.currency1)
        self.assertEqual(self.fxForward.currency2(), self.currency2)

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-10
        fair_nominal2 = self.fxForward.fairForwardRate().exchange(Money(self.currency1, self.nominal1)).value()
        fxForward = FxForward(self.nominal1, self.currency1,
                              fair_nominal2, self.currency2,
                              self.maturity_date, self.pay_currency1)
        fxForward.setPricingEngine(self.engine)
        self.assertFalse(abs(fxForward.NPV()) > tolerance)

        
class DepositTest(unittest.TestCase):
    def setUp(self):
        """ Set-up Deposit and engine. """
        self.trade_date = Date(6, October, 2018)
        Settings.instance().evaluationDate = self.trade_date
        self.nominal = 1000
        self.calendar = TARGET()
        self.bdc = ModifiedFollowing
        self.end_of_month = False
        self.day_counter = Actual365Fixed()
        self.rate = 0.02
        self.period = Period(3, Years)
        self.fixing_days = 2
        self.fixing_date = self.calendar.adjust(self.trade_date)
        self.start_date = self.calendar.advance(self.fixing_date, self.fixing_days, Days, self.bdc)
        self.maturity_date = self.calendar.advance(self.start_date, self.period, self.bdc)
        self.deposit = Deposit(self.nominal, self.rate, self.period,
                               self.fixing_days, self.calendar, self.bdc, 
                               self.end_of_month, self.day_counter, 
                               self.trade_date)
        self.flat_forward = FlatForward(self.trade_date, 0.03, self.day_counter)
        self.term_structure_handle = RelinkableYieldTermStructureHandle(self.flat_forward)
        self.engine = DepositEngine(self.term_structure_handle, False, self.trade_date, self.trade_date)
        self.deposit.setPricingEngine(self.engine)
  
    def testSimpleInspectors(self):
        """ Test Deposit simple inspectors. """
        self.assertEqual(self.deposit.fixingDate(), self.fixing_date)
        self.assertEqual(self.deposit.startDate(), self.start_date)
        self.assertEqual(self.deposit.maturityDate(), self.maturity_date)

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance = 1.0e-10
        fair_rate = self.deposit.fairRate()
        deposit = Deposit(self.nominal, fair_rate, self.period,
                          self.fixing_days, self.calendar, self.bdc, 
                          self.end_of_month, self.day_counter, 
                          self.trade_date)
        deposit.setPricingEngine(self.engine)
        self.assertFalse(abs(deposit.NPV()) > tolerance)
        

class EquityForwardTest(unittest.TestCase):
    def setUp(self):
        """ Set-up EquityForward and engine. """
        self.todays_date = Date(1, November, 2018)
        Settings.instance().evaluationDate = self.todays_date
        self.name = "UNILEVER"
        self.currency = GBPCurrency()
        self.position = Position.Long
        self.quantity = 100
        self.maturityDate = Date(1, November, 2019)
        self.strike = 60
        self.equityForward = EquityForward(self.name, self.currency, self.position,
                                           self.quantity, self.maturityDate, self.strike)
        self.tsDayCounter = Actual365Fixed()
        self.interest_rate = 0.03
        self.flatForward = FlatForward(self.todays_date, self.interest_rate, self.tsDayCounter)
        self.equityInterestRateCurve = RelinkableYieldTermStructureHandle()
        self.equityInterestRateCurve.linkTo(self.flatForward)
        self.dividendYieldCurve = RelinkableYieldTermStructureHandle(self.flatForward)
        self.discountCurve = RelinkableYieldTermStructureHandle(self.flatForward)
        self.equitySpotPrice = 60.0
        self.equitySpot = QuoteHandle(SimpleQuote(self.equitySpotPrice))
        self.includeSettlementDateFlows = True
        self.equityIndex2 = EquityIndex2(self.name, TARGET(), self.currency)
        self.equityIndex2Handle = RelinkableEquityIndex2Handle()
        self.equityIndex2Handle.linkTo(self.equityIndex2)

        self.engine = DiscountingEquityForwardEngine(self.equityIndex2Handle, self.discountCurve, self.includeSettlementDateFlows,
                                                     self.maturityDate, self.todays_date)
        self.equityForward.setPricingEngine(self.engine)
        
    def testSimpleInspectors(self):
        """ Test EquityForward simple inspectors. """
        self.assertEqual(self.strike, self.equityForward.strike())
        self.assertEqual(self.maturityDate, self.equityForward.maturityDate())
        self.assertEqual(self.quantity, self.equityForward.quantity())
        self.assertEqual(self.currency, self.equityForward.currency())

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance= 1.0e-10
        self.assertFalse(abs(self.equityForward.NPV()) > tolerance)
        
        
class PaymentTest(unittest.TestCase):
    def setUp(self):
        """ Test consistency of fair price and NPV() """
        self.todays_date = Date(1, November, 2018)
        Settings.instance().evaluationDate=self.todays_date
        self.currency = GBPCurrency()
        self.settlementDate = Date(1, November, 2019)
        self.day_counter = Actual365Fixed()
        self.nominal = 100.0
        self.spotFX = QuoteHandle(SimpleQuote(1))
        self.rate = 0.0
        self.flat_forward = FlatForward(self.todays_date, self.rate, self.day_counter)
        self.discount_curve = RelinkableYieldTermStructureHandle(self.flat_forward)
        self.payment = Payment(self.nominal, self.currency, self.settlementDate)
        self.includeSettlementDateFlows = True
        self.engine = PaymentDiscountingEngine(self.discount_curve, 
                                               self.spotFX, 
                                               self.includeSettlementDateFlows,
                                               self.settlementDate,
                                               self.todays_date)
        self.payment.setPricingEngine(self.engine)

        
    def testSimpleInspectors(self):
        """ Test Payment simple inspectors. """
        self.assertEqual(self.nominal, self.payment.cashFlow().amount())
        self.assertEqual(self.currency, self.payment.currency())

    def testConsistency(self):
        """ Test consistency of fair price and NPV() """
        tolerance= 1.0e-10
        self.assertFalse(abs(self.payment.NPV() - self.nominal) > tolerance)
        
if __name__ == '__main__':
    import ORE
    print('testing ORE ' + ORE.__version__)
    suite = unittest.TestSuite()
    suite.addTest(unittest.makeSuite(CrossCurrencyFixFloatSwapTest, 'test'))
    suite.addTest(unittest.makeSuite(CommodityForwardTest, 'test'))
    suite.addTest(unittest.makeSuite(SubPeriodsSwapTest, 'test'))
    suite.addTest(unittest.makeSuite(TenorBasisSwapTest, 'test'))
    suite.addTest(unittest.makeSuite(FxForwardTest, 'test'))
    suite.addTest(unittest.makeSuite(DepositTest, 'test'))
    suite.addTest(unittest.makeSuite(EquityForwardTest, 'test'))
    suite.addTest(unittest.makeSuite(PaymentTest, 'test'))
    suite.addTest(unittest.makeSuite(AverageOISTest, 'test'))
    suite.addTest(unittest.makeSuite(OvernightIndexedCrossCcyBasisSwapTest, 'test'))
    suite.addTest(unittest.makeSuite(CreditDefaultSwapTest, 'test'))
    suite.addTest(unittest.makeSuite(CDSOptionTest, 'test'))
    suite.addTest(unittest.makeSuite(CrossCcyBasisSwapTest, 'test'))
    unittest.TextTestRunner(verbosity=2).run(suite)

