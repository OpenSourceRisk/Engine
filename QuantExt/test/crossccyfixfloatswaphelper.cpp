/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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
#include <boost/make_shared.hpp>
#include <boost/test/unit_test.hpp>
#include <ql/currencies/all.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/types.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/termstructures/crossccyfixfloatswaphelper.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {

struct CommonVars {

    Date asof;
    Natural settlementDays;
    Calendar payCalendar;
    BusinessDayConvention payConvention;
    Natural payLag;
    Period tenor;
    Currency fixedCurrency;
    Frequency fixedFrequency;
    DayCounter fixedDayCount;
    Real usdNominal;

    // 5Y TRY annual fixed rate vs. USD 3M Libor
    Handle<Quote> rate;
    // USD/TRY spot FX rate
    QuantLib::ext::shared_ptr<SimpleQuote> spotFx;
    // Spread on float leg of swap
    QuantLib::ext::shared_ptr<SimpleQuote> spread;
    // USD Libor 3M projection curve
    Handle<YieldTermStructure> liborProjCurve;
    // USD Libor 3M index
    QuantLib::ext::shared_ptr<IborIndex> index;
    // USD discount curve
    Handle<YieldTermStructure> usdDiscCurve;

    // Hold the helper created during testing
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwapHelper> helper;

    CommonVars() {
        asof = Date(11, Sep, 2018);
        settlementDays = 2;
        payCalendar = JointCalendar(UnitedStates(UnitedStates::Settlement), UnitedKingdom(), Turkey());
        payConvention = Following;
        payLag = 0;
        tenor = 5 * Years;
        fixedCurrency = TRYCurrency();
        fixedFrequency = Annual;
        fixedDayCount = Actual360();
        usdNominal = 10000000.0;

        rate = Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.25));
        spotFx = QuantLib::ext::make_shared<SimpleQuote>(6.4304);
        spread = QuantLib::ext::make_shared<SimpleQuote>(0.0);
        liborProjCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.029773, Actual365Fixed()));
        index = QuantLib::ext::make_shared<USDLibor>(3 * Months, liborProjCurve);
        usdDiscCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), 0.026727, Actual365Fixed()));
    }
};

QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> makeTestSwap(const CommonVars& vars,
                                                     const Handle<YieldTermStructure>& discCurve) {

    // Swap start and end date
    Date referenceDate = Settings::instance().evaluationDate();
    referenceDate = vars.payCalendar.adjust(referenceDate);
    Date start = vars.payCalendar.advance(referenceDate, vars.settlementDays * Days);
    Date end = start + vars.tenor;

    // Fixed TRY schedule
    Schedule fixedSchedule(start, end, Period(vars.fixedFrequency), vars.payCalendar, vars.payConvention,
                           vars.payConvention, DateGeneration::Backward, false);

    // Float USD schedule
    Schedule floatSchedule(start, end, vars.index->tenor(), vars.payCalendar, vars.payConvention, vars.payConvention,
                           DateGeneration::Backward, false);

    // Create swap
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap(new CrossCcyFixFloatSwap(
        CrossCcyFixFloatSwap::Payer, vars.usdNominal * vars.spotFx->value(), vars.fixedCurrency, fixedSchedule,
        vars.rate->value(), vars.fixedDayCount, vars.payConvention, vars.payLag, vars.payCalendar, vars.usdNominal,
        vars.index->currency(), floatSchedule, vars.index, vars.spread->value(), vars.payConvention, vars.payLag,
        vars.payCalendar));

    // Attach pricing engine
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<CrossCcySwapEngine>(
        vars.fixedCurrency, discCurve, vars.index->currency(), vars.usdDiscCurve, Handle<Quote>(vars.spotFx));
    swap->setPricingEngine(engine);

    return swap;
}

// Use the helper that we are testing to create a bootstrapped curve
Handle<YieldTermStructure> bootstrappedCurve(CommonVars& vars) {

    // Create a helper
    vector<QuantLib::ext::shared_ptr<RateHelper> > helpers(1);
    vars.helper.reset(new CrossCcyFixFloatSwapHelper(
        vars.rate, Handle<Quote>(vars.spotFx), vars.settlementDays, vars.payCalendar, vars.payConvention, vars.tenor,
        vars.fixedCurrency, vars.fixedFrequency, vars.payConvention, vars.fixedDayCount, vars.index, vars.usdDiscCurve,
        Handle<Quote>(vars.spread)));
    helpers[0] = vars.helper;

    // Create a yield curve referencing the helper
    return Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear> >(0, NullCalendar(), helpers, Actual365Fixed()));
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CrossCurrencyFixFloatSwapHelperTest)

BOOST_AUTO_TEST_CASE(testBootstrap) {

    BOOST_TEST_MESSAGE("Test simple bootstrap against cross currency fix float swap");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> tryDiscCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap = makeTestSwap(vars, tryDiscCurve);

    // Swap should have NPV = 0.0. On notional = $10M i.e. TRY60.5M, 1e-5 is enough.
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check fair fixed rates match. Bootstrap uses 1e-12 accuracy.
    Real relTol = 1e-10;
    BOOST_CHECK_CLOSE(vars.rate->value(), swap->fairFixedRate(), relTol);

    // Check the 5Y discount factor
    DiscountFactor expDisc = 0.3299260408883904;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);
}

BOOST_AUTO_TEST_CASE(testSpotFxChange) {

    BOOST_TEST_MESSAGE("Test rebootstrap under spot FX change");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> tryDiscCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap = makeTestSwap(vars, tryDiscCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor
    Real relTol = 1e-10;
    DiscountFactor expDisc = 0.3299260408883904;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the nominal of the helper swap
    BOOST_CHECK_CLOSE(vars.spotFx->value(), vars.helper->swap()->fixedNominal(), relTol);

    // Bump the spot rate by 10%
    vars.spotFx->setValue(vars.spotFx->value() * 1.1);

    // Build a new swap using the updated spot FX rate
    swap = makeTestSwap(vars, tryDiscCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. It should be the same.
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the nominal of the helper swap. Should now be the bumped amount
    BOOST_CHECK_CLOSE(vars.spotFx->value(), vars.helper->swap()->fixedNominal(), relTol);
}

BOOST_AUTO_TEST_CASE(testSpreadChange) {

    BOOST_TEST_MESSAGE("Test rebootstrap under helper spread change");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> tryDiscCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap = makeTestSwap(vars, tryDiscCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor
    Real relTol = 1e-10;
    DiscountFactor expDisc = 0.3299260408883904;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the spread of the helper swap
    BOOST_CHECK_CLOSE(vars.spread->value(), vars.helper->swap()->floatSpread(), relTol);

    // Add a 10bps spread
    vars.spread->setValue(0.0010);

    // Build a new swap using the updated spread of 10bps
    swap = makeTestSwap(vars, tryDiscCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. Added spread on float => higher discount factor.
    expDisc = 0.3322218009717460;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the spread of the helper swap. Should now be 10bps.
    BOOST_CHECK_CLOSE(vars.spread->value(), vars.helper->swap()->floatSpread(), relTol);
}

BOOST_AUTO_TEST_CASE(testMovingEvaluationDate) {

    BOOST_TEST_MESSAGE("Test rebootstrap after moving evaluation date");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> tryDiscCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyFixFloatSwap> swap = makeTestSwap(vars, tryDiscCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor
    Real relTol = 1e-10;
    DiscountFactor expDisc = 0.3299260408883904;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the start date of the helper swap
    BOOST_CHECK_EQUAL(swap->startDate(), vars.helper->swap()->startDate());

    // Move evaluation date forward
    vars.asof = vars.asof + 1 * Days;
    Settings::instance().evaluationDate() = vars.asof;

    // Build a new swap using new evaluation date
    swap = makeTestSwap(vars, tryDiscCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. Changes slightly due to helper holidays/weekends.
    expDisc = 0.3299334970640459;
    BOOST_CHECK_CLOSE(expDisc, tryDiscCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the start date of the helper swap. Should be 1 day greater.
    BOOST_CHECK_EQUAL(swap->startDate(), vars.helper->swap()->startDate());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
