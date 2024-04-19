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
#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/instruments/bonds/fixedratebond.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/schedule.hpp>
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
    Handle<Quote> rateQuote(QuantLib::ext::make_shared<SimpleQuote>(0.1));
    Handle<Quote> issuerSpreadQuote(QuantLib::ext::make_shared<SimpleQuote>(0.0));
    DayCounter dc = Actual365Fixed();
    Handle<YieldTermStructure> yts(QuantLib::ext::make_shared<FlatForward>(today, rateQuote, dc, Compounded, Semiannual));
    Handle<DefaultProbabilityTermStructure> dpts(QuantLib::ext::make_shared<FlatHazardRate>(today, issuerSpreadQuote, dc));
    Handle<Quote> bondSpecificSpread(QuantLib::ext::make_shared<SimpleQuote>(0.0));

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

    // bond option market data
    // discount curve
    Handle<Quote> discountQuote(QuantLib::ext::make_shared<SimpleQuote>(0.1));
    Handle<YieldTermStructure> discountTS(
        QuantLib::ext::make_shared<FlatForward>(today, discountQuote, dc, Compounded, Semiannual));

    // lognormal yield vola
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs(
        new QuantLib::ConstantSwaptionVolatility(Settings::instance().evaluationDate(), NullCalendar(),
                                                 ModifiedFollowing, 0.5, Actual365Fixed(), ShiftedLognormal, 0.0));

    // pricing engine with lognormal yield vola
    QuantLib::ext::shared_ptr<PricingEngine> bondOptionEngine(new QuantExt::BlackBondOptionEngine(
        discountTS, Handle<QuantLib::SwaptionVolatilityStructure>(svs), discountTS));

    // build bond option
    Real strikePrice = notional;
    Real faceAmount = notional;
    Natural settlementDays = 2;
    std::vector<Rate> rates = std::vector<Rate>(1, couponRate);
    QuantLib::Bond::Price callabilityPrice = QuantLib::Bond::Price(strikePrice, QuantLib::Bond::Price::Dirty);
    Callability::Type callabilityType = Callability::Call;
    Date exerciseDate = Date(5, Dec, 2016);
    QuantLib::ext::shared_ptr<Callability> callability(new Callability(callabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule callabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, callability);

    QuantLib::ext::shared_ptr<QuantLib::Bond> bond(
        new QuantLib::FixedRateBond(settlementDays, faceAmount, schedule, rates, dc, bdc, redemption, issueDate));
    QuantLib::ext::shared_ptr<QuantExt::BondOption> bondOption(new QuantExt::BondOption(bond, callabilitySchedule));
    bondOption->setPricingEngine(bondOptionEngine);

    // build tief OTM bond option
    Real otmStrikePrice = notional * 2;
    QuantLib::Bond::Price otmCallabilityPrice = QuantLib::Bond::Price(otmStrikePrice, QuantLib::Bond::Price::Dirty);
    QuantLib::ext::shared_ptr<Callability> otmCallability(new Callability(otmCallabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule otmCallabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, otmCallability);

    QuantLib::ext::shared_ptr<QuantExt::BondOption> otmBondOption(new QuantExt::BondOption(bond, otmCallabilitySchedule));
    otmBondOption->setPricingEngine(bondOptionEngine);

    // build tief ITM bond option
    Real itmStrikePrice = notional / 2;
    QuantLib::Bond::Price itmCallabilityPrice = QuantLib::Bond::Price(itmStrikePrice, QuantLib::Bond::Price::Dirty);
    QuantLib::ext::shared_ptr<Callability> itmCallability(new Callability(itmCallabilityPrice, callabilityType, exerciseDate));
    CallabilitySchedule itmCallabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, itmCallability);

    QuantLib::ext::shared_ptr<QuantExt::BondOption> itmBondOption(new QuantExt::BondOption(bond, itmCallabilitySchedule));
    itmBondOption->setPricingEngine(bondOptionEngine);

    // build zero bond option
    QuantLib::ext::shared_ptr<QuantLib::Bond> zerobond(new QuantLib::FixedRateBond(
        settlementDays, faceAmount,
        Schedule(startDate, endDate, Period(Once), calendar, bdc, bdcEnd, DateGeneration::Backward, false),
        std::vector<Rate>(1, 0.0), dc, bdc, redemption, issueDate));
    QuantLib::ext::shared_ptr<QuantExt::BondOption> zeroBondOption(new QuantExt::BondOption(zerobond, callabilitySchedule));
    zeroBondOption->setPricingEngine(bondOptionEngine);

    BOOST_TEST_MESSAGE("normal bond option price = " << bondOption->NPV());
    BOOST_CHECK_CLOSE(bondOption->NPV(), 36.807084355035521, 0.000001);

    BOOST_TEST_MESSAGE("tief otm bond option price = " << otmBondOption->NPV());
    BOOST_CHECK_CLOSE(otmBondOption->NPV(), 3.2657301416105546e-45, 0.000001);

    BOOST_TEST_MESSAGE("tief itm bond option price = " << itmBondOption->NPV());
    BOOST_CHECK_CLOSE(itmBondOption->NPV(), 491.52718033161705, 0.000001);

    BOOST_TEST_MESSAGE("zero bond option price = " << zeroBondOption->NPV());
    BOOST_CHECK_CLOSE(zeroBondOption->NPV(), 0.15813277744399326, 0.000001);

    // test put-call parity
    Real putCallStrikePrice = 1000;
    QuantLib::Bond::Price putCallCallabilityPrice =
        QuantLib::Bond::Price(putCallStrikePrice, QuantLib::Bond::Price::Dirty);

    QuantLib::ext::shared_ptr<Callability> callCallability(
        new Callability(putCallCallabilityPrice, Callability::Call, exerciseDate));
    CallabilitySchedule callCallabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, callCallability);
    QuantLib::ext::shared_ptr<QuantExt::BondOption> bondCallOption(new QuantExt::BondOption(bond, callCallabilitySchedule));
    bondCallOption->setPricingEngine(bondOptionEngine);

    QuantLib::ext::shared_ptr<Callability> putCallability(
        new Callability(putCallCallabilityPrice, Callability::Put, exerciseDate));
    CallabilitySchedule putCallabilitySchedule = std::vector<QuantLib::ext::shared_ptr<Callability>>(1, putCallability);
    QuantLib::ext::shared_ptr<QuantExt::BondOption> bondPutOption(new QuantExt::BondOption(bond, putCallabilitySchedule));
    bondPutOption->setPricingEngine(bondOptionEngine);

    Real discount = discountTS->discount(exerciseDate);
    // forward bond price, read from the pricing engine
    Real fwdbond = bondCallOption->result<Real>("FwdCashPrice");
    BOOST_TEST_MESSAGE("bond forward price = " << fwdbond);

    BOOST_TEST_MESSAGE("bond call option price = " << bondCallOption->NPV());
    BOOST_TEST_MESSAGE("bond put option price = " << bondPutOption->NPV());

    Real right = bondCallOption->NPV() + putCallStrikePrice * discount;
    Real left = bondPutOption->NPV() + fwdbond * discount;

    BOOST_TEST_MESSAGE("right side of put-call parity (call + K*df) = " << right);
    BOOST_TEST_MESSAGE("left side of put-call parity (put + Fwd*df) = " << left);
    BOOST_CHECK_CLOSE(right, left, 0.000001);

    // test put-call parity with normal yield vola
    // normal yield vola
    QuantLib::ext::shared_ptr<QuantLib::SwaptionVolatilityStructure> svs_normal(new QuantLib::ConstantSwaptionVolatility(
        Settings::instance().evaluationDate(), NullCalendar(), ModifiedFollowing, 0.5, Actual365Fixed(), Normal));

    // pricing engine with lognormal yield vola
    QuantLib::ext::shared_ptr<PricingEngine> bondOptionEngineNormal(new QuantExt::BlackBondOptionEngine(
        discountTS, Handle<QuantLib::SwaptionVolatilityStructure>(svs_normal), discountTS));

    bondCallOption->setPricingEngine(bondOptionEngineNormal);
    bondPutOption->setPricingEngine(bondOptionEngineNormal);

    BOOST_TEST_MESSAGE("bond call option price = " << bondCallOption->NPV());
    BOOST_TEST_MESSAGE("bond put option price = " << bondPutOption->NPV());

    right = bondCallOption->NPV() + putCallStrikePrice * discount;
    left = bondPutOption->NPV() + fwdbond * discount;

    BOOST_TEST_MESSAGE("right side of put-call parity (call + K*df) = " << right);
    BOOST_TEST_MESSAGE("left side of put-call parity (put + Fwd*df) = " << left);
    BOOST_CHECK_CLOSE(right, left, 0.000001);
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
