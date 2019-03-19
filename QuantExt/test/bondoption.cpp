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

#include "utilities.hpp"
#include <boost/test/unit_test.hpp>
#include "toplevelfixture.hpp"
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
#include <ql/instruments/bond.hpp>
#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <qle/pricingengines/discountingriskybondengine.hpp>

#include <boost/make_shared.hpp>
#include <iomanip>
#include <iostream>


using namespace QuantLib;
using namespace QuantExt;

using boost::unit_test_framework::test_suite;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(BondOptionTest)

BOOST_AUTO_TEST_CASE(testBondOption) {

    BOOST_TEST_MESSAGE("Testing QuantExt Bond Option pricing.");

    SavedSettings backup;

    Settings::instance().evaluationDate() = Date(5, Feb, 2016);
    Date issueDate = Date(3, Nov, 2015);
    Date today = Settings::instance().evaluationDate();
    Settings::instance().includeReferenceDateEvents() = true;

    // bond market data
    Handle<Quote> rateQuote(boost::make_shared<SimpleQuote>(0.1));
    Handle<Quote> issuerSpreadQuote(boost::make_shared<SimpleQuote>(0.0));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(boost::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(boost::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(boost::make_shared<SimpleQuote>(0.0));

    // build the underlying fixed rate bond
    Date startDate = today;
    Date endDate = startDate + Period(2, Years);
    Period tenor = 6 * Months;
    Calendar calendar = WeekendsOnly();
    BusinessDayConvention bdc = Following;
    BusinessDayConvention bdcEnd = bdc;
    DateGeneration::Rule rule = DateGeneration::Forward;
    bool endOfMonth = false;
    Date firstDate, lastDate;
    Schedule schedule(startDate, endDate, tenor, calendar, bdc, bdcEnd, rule, endOfMonth, firstDate, lastDate);

    Real redemption = 100.0;
    Real notional = 1000;
    Real couponRate = 0.1;

    Leg leg =
        FixedRateLeg(schedule).withNotionals(notional).withCouponRates(couponRate, dc).withPaymentAdjustment(bdc);

    // bond option market data
    Handle<Quote> discountQuote(boost::make_shared<SimpleQuote>(0.1));
    Handle<YieldTermStructure> discountTS(
        boost::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));

    boost::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
        new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
            ModifiedFollowing, 0.5, Actual365Fixed(), ShiftedLognormal, 0.0));

    boost::shared_ptr<PricingEngine> bondOptionEngine(new QuantExt::BlackBondOptionEngine(
        Handle<QuantLib::SwaptionVolatilityStructure>(svs), discountTS));

    // build bond option
    Date maturityDate = today;
    Real strikePrice = notional;
    Real faceAmount = notional;
    Natural settlementDays = 2;
    std::vector<Rate> rates = std::vector<Rate>(1, couponRate);
    Callability::Price callabilityPrice = Callability::Price(strikePrice, Callability::Price::Dirty);
    Callability::Type callabilityType = Callability::Call;
    Date exerciseDate = Date(5, Dec, 2016);
    boost::shared_ptr<Callability> callability(new Callability(callabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule callabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, callability);

    boost::shared_ptr<QuantExt::BondOption> bondOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
        dc, bdc, redemption, issueDate, callabilitySchedule));
    bondOption->setPricingEngine(bondOptionEngine);

    // build tief OTM bond option
    Real otmStrikePrice = notional * 2;
    Callability::Price otmCallabilityPrice = Callability::Price(otmStrikePrice, Callability::Price::Dirty);
    boost::shared_ptr<Callability> otmCallability(new Callability(otmCallabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule otmCallabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, otmCallability);

    boost::shared_ptr<QuantExt::BondOption> otmBondOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
        dc, bdc, redemption, issueDate, otmCallabilitySchedule));
    otmBondOption->setPricingEngine(bondOptionEngine);

    // build tief ITM bond option
    Real itmStrikePrice = notional / 2;
    Callability::Price itmCallabilityPrice = Callability::Price(itmStrikePrice, Callability::Price::Dirty);
    boost::shared_ptr<Callability> itmCallability(new Callability(itmCallabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule itmCallabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, itmCallability);

    boost::shared_ptr<QuantExt::BondOption> itmBondOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
        dc, bdc, redemption, issueDate, itmCallabilitySchedule));
    itmBondOption->setPricingEngine(bondOptionEngine);

    // build zero bond option
    boost::shared_ptr<QuantExt::BondOption> zeroBondOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount,
        Schedule(startDate, endDate, Period(Once), calendar, bdc, bdcEnd, DateGeneration::Backward, false),
        std::vector<Rate>(1, 0.0), dc, bdc, redemption, issueDate, callabilitySchedule));
    zeroBondOption->setPricingEngine(bondOptionEngine);

    BOOST_TEST_MESSAGE("normal bond option price = " << bondOption->NPV());
    BOOST_CHECK_CLOSE(bondOption->NPV(), 41.130585192314584, 0.000001);

    BOOST_TEST_MESSAGE("tief otm bond option price = " << otmBondOption->NPV());
    BOOST_CHECK_CLOSE(otmBondOption->NPV(), 5.8386788966340013e-43, 0.000001);

    BOOST_TEST_MESSAGE("tief itm bond option price = " << itmBondOption->NPV());
    BOOST_CHECK_CLOSE(itmBondOption->NPV(), 534.24837517090668, 0.000001);

    BOOST_TEST_MESSAGE("zero bond option price = " << zeroBondOption->NPV());
    BOOST_CHECK_CLOSE(zeroBondOption->NPV(), 0.18275705142679932, 0.000001);

    // test put-call parity
    Real putCallStrikePrice = 1000;
    Callability::Price putCallCallabilityPrice = Callability::Price(putCallStrikePrice, Callability::Price::Dirty);

    boost::shared_ptr<Callability> callCallability(new Callability(putCallCallabilityPrice, Callability::Call, exerciseDate));
    CallabilitySchedule callCallabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, callCallability);
    boost::shared_ptr<QuantExt::BondOption> bondCallOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
        dc, bdc, redemption, issueDate, callCallabilitySchedule));
    bondCallOption->setPricingEngine(bondOptionEngine);

    boost::shared_ptr<Callability> putCallability(new Callability(putCallCallabilityPrice, Callability::Put, exerciseDate));
    CallabilitySchedule putCallabilitySchedule = std::vector<boost::shared_ptr<Callability> >(1, putCallability);
    boost::shared_ptr<QuantExt::BondOption> bondPutOption(new QuantExt::FixedRateBondOption(settlementDays, faceAmount, schedule, rates,
        dc, bdc, redemption, issueDate, putCallabilitySchedule));
    bondPutOption->setPricingEngine(bondOptionEngine);

    Real discount = discountTS->discount(today);
    // forwar bond price, got from the pricing engine
    Real fwdbond = 1034.2483751709069;

    BOOST_TEST_MESSAGE("bond call option price = " << bondCallOption->NPV());
    BOOST_TEST_MESSAGE("bond put option price = " << bondPutOption->NPV());

    Real right = bondCallOption->NPV() + putCallStrikePrice * discount;
    Real left = bondPutOption->NPV() + fwdbond * discount;

    BOOST_TEST_MESSAGE("right side of put-call parity (call + K*df) = " << right);
    BOOST_TEST_MESSAGE("left side of put-call parity (put + Fwd*df) = " << left);
    BOOST_CHECK_CLOSE(right, left, 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()


