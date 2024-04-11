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
#include <ql/indexes/ibor/gbplibor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/discountcurve.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewiseyieldcurve.hpp>
#include <ql/time/calendars/all.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/types.hpp>
#include <qle/pricingengines/crossccyswapengine.hpp>
#include <qle/termstructures/crossccybasismtmresetswaphelper.hpp>

using namespace std;
using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;

namespace {

struct CommonVars {

    Date asof;
    Natural settlementDays;
    Calendar domesticCalendar, foreignCalendar, payCalendar;
    BusinessDayConvention payConvention;
    Natural payLag;
    Period tenor;
    Currency foreignCurrency, domesticCurrency;
    DayCounter dayCount;
    Real foreignNominal;
    QuantLib::ext::shared_ptr<SimpleQuote> spotFxQuote;
    QuantLib::ext::shared_ptr<SimpleQuote> spreadQuote;
    Handle<YieldTermStructure> domesticProjCurve;
    Handle<YieldTermStructure> domesticDiscCurve;
    Handle<YieldTermStructure> foreignProjCurve;
    QuantLib::ext::shared_ptr<IborIndex> domesticIndex;
    QuantLib::ext::shared_ptr<IborIndex> foreignIndex;
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwapHelper> helper;

    CommonVars() {
        asof = Date(11, Sep, 2018);
        settlementDays = 2;
        domesticCalendar = UnitedStates(UnitedStates::Settlement);
        foreignCalendar = UnitedKingdom();
        payCalendar = JointCalendar(domesticCalendar, foreignCalendar);
        payConvention = Following;
        payLag = 0;
        tenor = 5 * Years;
        domesticCurrency = USDCurrency();
        foreignCurrency = GBPCurrency();
        dayCount = Actual360();
        foreignNominal = 10000000.0;
        spotFxQuote = QuantLib::ext::make_shared<SimpleQuote>(1.2);
        spreadQuote = QuantLib::ext::make_shared<SimpleQuote>(-0.0015);

        // curves
        domesticProjCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, domesticCalendar, 0.02, Actual365Fixed()));
        foreignProjCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, foreignCalendar, 0.03, Actual365Fixed()));
        domesticDiscCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, domesticCalendar, 0.01, Actual365Fixed()));

        // indices
        foreignIndex = QuantLib::ext::make_shared<GBPLibor>(3 * Months, foreignProjCurve);
        domesticIndex = QuantLib::ext::make_shared<USDLibor>(3 * Months, domesticProjCurve);
    }
};

QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> makeTestSwap(const CommonVars& vars,
                                                          const Handle<YieldTermStructure>& foreignDiscCurve) {
    // Swap schedule
    Date referenceDate = Settings::instance().evaluationDate();
    referenceDate = vars.payCalendar.adjust(referenceDate);
    Date start = vars.payCalendar.advance(referenceDate, vars.settlementDays * Days);
    Date end = start + vars.tenor;
    Schedule schedule(start, end, 3 * Months, vars.payCalendar, vars.payConvention, vars.payConvention,
                      DateGeneration::Backward, false);
    // Create swap
    QuantLib::ext::shared_ptr<FxIndex> fxIndex = QuantLib::ext::make_shared<FxIndex>(
        "dummy", vars.settlementDays, vars.foreignCurrency, vars.domesticCurrency, vars.payCalendar,
        Handle<Quote>(vars.spotFxQuote), foreignDiscCurve, vars.domesticDiscCurve);
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap(new CrossCcyBasisMtMResetSwap(
        vars.foreignNominal, vars.foreignCurrency, schedule, vars.foreignIndex, vars.spreadQuote->value(),
        vars.domesticCurrency, schedule, vars.domesticIndex, 0.0, fxIndex, false));
    // Attach pricing engine
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<CrossCcySwapEngine>(vars.domesticCurrency, vars.domesticDiscCurve, vars.foreignCurrency,
                                               foreignDiscCurve, Handle<Quote>(vars.spotFxQuote));
    swap->setPricingEngine(engine);
    return swap;
}

// Use helper to build bootstrapped curve
Handle<YieldTermStructure> bootstrappedCurve(CommonVars& vars) {

    // Create a helper
    vector<QuantLib::ext::shared_ptr<RateHelper> > helpers(1);
    vars.helper.reset(new CrossCcyBasisMtMResetSwapHelper(
        Handle<Quote>(vars.spreadQuote), Handle<Quote>(vars.spotFxQuote), vars.settlementDays, vars.payCalendar,
        vars.tenor, vars.payConvention, vars.foreignIndex, vars.domesticIndex, Handle<YieldTermStructure>(),
        vars.domesticDiscCurve));
    helpers[0] = vars.helper;

    // Build yield curve referencing the helper
    return Handle<YieldTermStructure>(
        QuantLib::ext::make_shared<PiecewiseYieldCurve<Discount, LogLinear> >(0, NullCalendar(), helpers, Actual365Fixed()));
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CrossCcyBasisMtMResetSwapHelperTest)

BOOST_AUTO_TEST_CASE(testBootstrap) {

    BOOST_TEST_MESSAGE("Test simple bootstrap against cross currency MtM resetting swap");

    SavedSettings backup;
    CommonVars vars;
    Settings::instance().evaluationDate() = vars.asof;

    // Create bootstrapped curve
    Handle<YieldTermStructure> discCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap = makeTestSwap(vars, discCurve);

    // Swap should have NPV = 0.0.
    Real tol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), tol);

    // Check fair spreads match. Bootstrap uses 1e-12 accuracy.
    Real relTol = 1e-10;
    BOOST_CHECK_CLOSE(vars.spreadQuote->value(), swap->fairForeignSpread(), relTol);

    // Check the 5Y discount factor
    DiscountFactor expDisc = 0.91155524911218166;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);
}

BOOST_AUTO_TEST_CASE(testSpotFxChange) {

    BOOST_TEST_MESSAGE("Test rebootstrap under spot FX change");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> discCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap = makeTestSwap(vars, discCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check fair spreads match. Bootstrap uses 1e-12 accuracy.
    Real relTol = 1e-10;
    BOOST_CHECK_CLOSE(vars.spreadQuote->value(), swap->fairForeignSpread(), relTol);

    // Check the 5Y discount factor
    DiscountFactor expDisc = 0.91155524911218166;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the nominal of the helper swap
    BOOST_CHECK_CLOSE(vars.spotFxQuote->value(), std::fabs(vars.helper->swap()->leg(2).front()->amount()), relTol);

    // Bump the spot rate by 10%
    vars.spotFxQuote->setValue(vars.spotFxQuote->value() * 1.1);

    // Build a new swap using the updated spot FX rate
    swap = makeTestSwap(vars, discCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. It should be the same.
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the nominal of the helper swap. Should now be the bumped amount
    BOOST_CHECK_CLOSE(vars.spotFxQuote->value(), std::fabs(vars.helper->swap()->leg(2).front()->amount()), relTol);
}

BOOST_AUTO_TEST_CASE(testSpreadChange) {

    BOOST_TEST_MESSAGE("Test rebootstrap under helper spread change");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> discCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap = makeTestSwap(vars, discCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check fair spreads match. Bootstrap uses 1e-12 accuracy.
    Real relTol = 1e-10;
    BOOST_CHECK_CLOSE(vars.spreadQuote->value(), swap->fairForeignSpread(), relTol);

    // Check the 5Y discount factor
    DiscountFactor expDisc = 0.91155524911218166;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Add a 10bps spread
    vars.spreadQuote->setValue(0.0015);

    // Build a new swap using the updated spread of 10bps
    swap = makeTestSwap(vars, discCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. More spread on foreign leg => more significant discount factor.
    expDisc = 0.89807807922008731;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the spread of the helper swap. Should now be 15bps.
    BOOST_CHECK_CLOSE(vars.spreadQuote->value(), swap->fairForeignSpread(), relTol);
}

BOOST_AUTO_TEST_CASE(testMovingEvaluationDate) {

    BOOST_TEST_MESSAGE("Test rebootstrap after moving evaluation date");

    SavedSettings backup;

    CommonVars vars;

    Settings::instance().evaluationDate() = vars.asof;

    // Create a helper and bootstrapped curve
    Handle<YieldTermStructure> discCurve = bootstrappedCurve(vars);

    // Create the helper swap manually and price it using curve bootstrapped from helper
    QuantLib::ext::shared_ptr<CrossCcyBasisMtMResetSwap> swap = makeTestSwap(vars, discCurve);

    // Check NPV = 0.0
    Real absTol = 1e-5;
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check fair spreads match. Bootstrap uses 1e-12 accuracy.
    Real relTol = 1e-10;
    BOOST_CHECK_CLOSE(vars.spreadQuote->value(), swap->fairForeignSpread(), relTol);

    // Check the 5Y discount factor
    DiscountFactor expDisc = 0.91155524911218166;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the start date of the helper swap
    BOOST_CHECK_EQUAL(swap->startDate(), vars.helper->swap()->startDate());

    // Move evaluation date forward
    vars.asof = vars.asof + 1 * Days;
    Settings::instance().evaluationDate() = vars.asof;

    // Build a new swap using new evaluation date
    swap = makeTestSwap(vars, discCurve);

    // Check that the new swap's NPV is 0.0
    BOOST_CHECK_SMALL(swap->NPV(), absTol);

    // Check the 5Y discount factor again. Changes slightly due to helper holidays/weekends.
    expDisc = 0.91155524848230363;
    BOOST_CHECK_CLOSE(expDisc, discCurve->discount(vars.asof + 5 * Years), relTol);

    // Check the start date of the helper swap. Should be 1 day greater.
    BOOST_CHECK_EQUAL(swap->startDate(), vars.helper->swap()->startDate());
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
