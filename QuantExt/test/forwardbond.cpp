/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "toplevelfixture.hpp"
#include <boost/test/unit_test.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
#include <qle/instruments/forwardbond.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <boost/make_shared.hpp>
#include <iomanip>

using namespace boost::unit_test_framework;
using namespace QuantLib;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(ForwardBondTest)

BOOST_AUTO_TEST_CASE(testForwardBond1) { // test if bond and forwardbond comp coincide at maturity
    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing. Case 1");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.005));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    bool settlementDirty = true;
    Real compensationPayment = 0.0;
    Date compensationPaymentDate = today;

    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery;
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 1 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));
    // Handle<Quote> recoveryRate(QuantLib::ext::make_shared<SimpleQuote>(0.4));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today;
    Real strikePrice = 103.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    // Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?
    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));
    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price = " << fwdBond->NPV());
    BOOST_CHECK_CLOSE(fwdBond->NPV() + strikePrice, bond->dirtyPrice(), 0.000001);
}

BOOST_AUTO_TEST_CASE(testForwardBond2) { // same testcase as above, but different setting of params

    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing. Case 2");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.005));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 95.0;
    Real couponRate = 0.04;
    bool settlementDirty = true;
    Real compensationPayment = 0.0;
    Date compensationPaymentDate = today;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery; /*Handle<Quote> recovery(QuantLib::ext::make_shared<SimpleQuote>(0.4));*/
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 2 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today;
    Real strikePrice = 98.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    // Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price = " << fwdBond->NPV());
    BOOST_CHECK_CLOSE(fwdBond->NPV() + strikePrice, bond->NPV(), 0.000001);
}

BOOST_AUTO_TEST_CASE(testForwardBond3) { // now true forward bond, but coupon payments are excluded

    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing. Case 3");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.005));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    bool settlementDirty = true;
    Real compensationPayment = 0.0;
    Date compensationPaymentDate = today;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 2 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today + 2 * Months; // there are no cfs in these two months.
    Real strikePrice = 98.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    // Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price = " << fwdBond->NPV());
    BOOST_CHECK_CLOSE((fwdBond->NPV() / (discountTS->discount(fwdMaturityDate)) + strikePrice) *
                          (incomeTS->discount(fwdMaturityDate)),
                      bond->NPV(), 0.000001); // no differnce in cfs. only check compounding factors
}

BOOST_AUTO_TEST_CASE(testForwardBond4) { // now true forward bond, one coupon before maturity

    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing. Case 4");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.00));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.000));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    bool settlementDirty = true;
    Real compensationPayment = 0.0;
    Date compensationPaymentDate = today;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 2 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << std::setprecision(12) << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today + 7 * Months + 7 * Days; // there is one CF in those two months. today+7*Months happens
                                                          // to be a saturday, thus two days are added.

    Real strikePrice = 98.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    //  Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price = " << std::setprecision(12) << fwdBond->NPV());
    BOOST_CHECK_CLOSE((fwdBond->NPV() / (discountTS->discount(fwdMaturityDate)) + strikePrice) *
                          (incomeTS->discount(fwdMaturityDate)),
                      bond->NPV() - (0.04 * 100.0 * 182 / 365 * (yts->discount(Date(8, Jun, 2017))) *
                                     (dpts->survivalProbability(Date(8, Jun, 2017)))),
                      0.000001); // one cf before maturity factors; the cf occurs at today+6*Months
    BOOST_TEST_MESSAGE("Present discount factors:");

    BOOST_TEST_MESSAGE("ytsDiscountFactors = " << std::setprecision(12) << yts->discount(Date(8, Jun, 2017)));
    BOOST_TEST_MESSAGE("ytsDiscountFactors = " << std::setprecision(12)
                                               << yts->discount(Date(8, Jun, 2017) + 1 * Days));
    BOOST_TEST_MESSAGE("ytsDiscountFactors = " << std::setprecision(12)
                                               << yts->discount(Date(8, Jun, 2017) + 5 * Days));

    BOOST_TEST_MESSAGE("incDiscountFactors = " << std::setprecision(12) << incomeTS->discount(Date(8, Jun, 2017)));
    BOOST_TEST_MESSAGE("incDiscountFactors = " << std::setprecision(12)
                                               << incomeTS->discount(Date(8, Jun, 2017) + 1 * Days));
    BOOST_TEST_MESSAGE("incDiscountFactors = " << std::setprecision(12)
                                               << incomeTS->discount(Date(8, Jun, 2017) + 5 * Days));

    BOOST_TEST_MESSAGE("disDiscountFactors = " << std::setprecision(12) << discountTS->discount(Date(8, Jun, 2017)));
    BOOST_TEST_MESSAGE("disDiscountFactors = " << std::setprecision(12)
                                               << discountTS->discount(Date(8, Jun, 2017) + 1 * Days));
    BOOST_TEST_MESSAGE("disDiscountFactors = " << std::setprecision(12)
                                               << discountTS->discount(Date(8, Jun, 2017) + 5 * Days));
}

BOOST_AUTO_TEST_CASE(testForwardBond5) { // now true forward bond, one coupon before maturity

    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing. Case 5");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.00));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.000));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    bool settlementDirty = true;
    Real compensationPayment = 5.0;
    Date compensationPaymentDate = today + 2 * Days;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 2 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << std::setprecision(12) << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today + 7 * Months + 7 * Days; // there is one CF in those two months. today+7*Months happens
                                                          // to be a saturday, thus two days are added.

    Real strikePrice = 98.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    //  Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price = " << std::setprecision(12) << fwdBond->NPV());
    BOOST_CHECK_CLOSE(((fwdBond->NPV() + compensationPayment * discountTS->discount(compensationPaymentDate)) /
                           (discountTS->discount(fwdMaturityDate)) +
                       strikePrice) *
                          (incomeTS->discount(fwdMaturityDate)),
                      bond->NPV() - (0.04 * 100.0 * 182 / 365 * (yts->discount(Date(8, Jun, 2017))) *
                                     (dpts->survivalProbability(Date(8, Jun, 2017)))),
                      0.000001); // one cf before maturity factors; the cf occurs at today+6*Months
}

BOOST_AUTO_TEST_CASE(testForwardBond6) { // now true forward bond, but coupon payments are excluded

    BOOST_TEST_MESSAGE("Testing QuantExt Forward Bond pricing: Clean vs dirty Strike. Case 6");
    BOOST_TEST_MESSAGE("This is like case 3 but with clean strike. Check long/short in particular.");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(8, Dec, 2016);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.02));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.005));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(10, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real couponRate = 0.04;
    bool settlementDirty = false;
    Real compensationPayment = 0.0;
    Date compensationPaymentDate = today;
    Leg leg =
        FixedRateLeg(schedule).withNotionals(redemption).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(QuantLib::ext::make_shared<QuantLib::Bond>(0, WeekendsOnly(), today, leg));
    Handle<Quote> recovery(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    QuantLib::ext::shared_ptr<PricingEngine> pricingEngine(
        QuantLib::ext::make_shared<QuantExt::DiscountingRiskyBondEngine>(yts, dpts, recovery, bondSpecificSpread, 2 * Months));
    bond->setPricingEngine(pricingEngine);

    Real price = bond->NPV();
    BOOST_TEST_MESSAGE("Bond price = " << price);

    // additional forward bond market data
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.01));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));
    Handle<Quote> incomeQuote(QuantLib::ext::make_shared<SimpleQuote>(0.005));
    Handle<YieldTermStructure> incomeTS(
        QuantLib::ext::make_shared<FlatForward>(today, incomeQuote, dc, Compounded, Semiannual));

    // build the forward bond
    // Date valueDate = today;
    Date fwdMaturityDate = today + 2 * Months; // there are no cfs in these two months.
    Real strikePrice = 98.0;
    QuantLib::ext::shared_ptr<Payoff> payoff = QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Long, strikePrice);
    // Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond_sh(
        new QuantExt::ForwardBond(bond, payoff, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine_sh(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond_sh->setPricingEngine(fwdBondEngine_sh);

    BOOST_TEST_MESSAGE("Forward Bond price long = " << fwdBond_sh->NPV());

    Real fwdBondNPV = fwdBond_sh->NPV(); // save result of long computation.

    QuantLib::ext::shared_ptr<Payoff> payoff_sh =
        QuantLib::ext::make_shared<QuantExt::ForwardBondTypePayoff>(Position::Short, strikePrice);
    // Natural settlementDays = 0; // why do we need this if we specify the value date explicitly ?

    QuantLib::ext::shared_ptr<QuantExt::ForwardBond> fwdBond(
        new QuantExt::ForwardBond(bond, payoff_sh, fwdMaturityDate, fwdMaturityDate, true, settlementDirty,
                                  compensationPayment, compensationPaymentDate));

    QuantLib::ext::shared_ptr<PricingEngine> fwdBondEngine(new QuantExt::DiscountingForwardBondEngine(
        discountTS, incomeTS, yts, bondSpecificSpread, dpts, recovery, 2 * Months));
    fwdBond->setPricingEngine(fwdBondEngine);

    BOOST_TEST_MESSAGE("Forward Bond price short = " << fwdBond->NPV());

    BOOST_CHECK_CLOSE(fwdBondNPV, (-1.0) * fwdBond->NPV(), 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
