/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
// clang-format off
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
// clang-format on
#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/nullcalendar.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/pricingengines/analyticcashsettledeuropeanengine.hpp>
#include <qle/termstructures/pricecurve.hpp>

using namespace boost::unit_test_framework;
using namespace QuantLib;
using namespace QuantExt;
using std::fixed;
using std::map;
using std::setprecision;
using std::string;
using std::vector;

namespace bdata = boost::unit_test::data;

namespace {

// Create a flat yield term structure where DF(0, t) = exp (-r * t)
Handle<YieldTermStructure> flatYts(Rate r) {
    return Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), r, Actual365Fixed()));
}

// Create a process for the tests
QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> getProcess(Rate spot, Volatility vol, Rate r, Rate q) {

    // Set up the term structures
    Handle<Quote> spotQuote(QuantLib::ext::make_shared<SimpleQuote>(spot));
    Handle<YieldTermStructure> rTs = flatYts(r);
    Handle<YieldTermStructure> qTs = flatYts(q);
    Handle<BlackVolTermStructure> volTs(QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), vol, Actual365Fixed()));

    // Return the process
    return QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(spotQuote, qTs, rTs, volTs);
}

// Create a dummy price term structure
Handle<PriceTermStructure> priceTs() {
    vector<Period> tenors{ 0 * Days, 1 * Years };
    vector<Real> prices{ 60.0, 69.0 };
    return Handle<PriceTermStructure>(
        QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear> >(tenors, prices, Actual365Fixed(), USDCurrency()));
}

// Return a map containing all of the CashSettledEuropeanOption results
map<string, Real> results(const CashSettledEuropeanOption& option) {

    map<string, Real> mp;

    mp["npv"] = option.NPV();
    try {
        // might not be set by engine
        mp["delta"] = option.delta();
        mp["deltaForward"] = option.deltaForward();
        mp["elasticity"] = option.elasticity();
        mp["gamma"] = option.gamma();
        mp["theta"] = option.theta();
        mp["thetaPerDay"] = option.thetaPerDay();
        mp["vega"] = option.vega();
        mp["rho"] = option.rho();
        mp["dividendRho"] = option.dividendRho();
    } catch (...) {
    }

    return mp;
}

// Check option values on or after expiry date
void checkOptionValues(CashSettledEuropeanOption& option, Rate r, Real exercisePrice, Real tolerance = 1e-12) {

    // Discount factor from payment and time to payment date.
    auto yts = flatYts(r);
    DiscountFactor df_tp = yts->discount(option.paymentDate());
    Time tp = yts->timeFromReference(option.paymentDate());
    BOOST_TEST_MESSAGE("Discount factor from payment is: " << fixed << setprecision(12) << df_tp);

    // Value at exercise
    Real valueAtExpiry = (*option.payoff())(exercisePrice);

    // Check the results after manual exercise.
    map<string, Real> cashSettledResults = results(option);
    BOOST_REQUIRE(cashSettledResults.count("npv") == 1);
    for (const auto& kv : cashSettledResults) {
        BOOST_TEST_CONTEXT("With result " << kv.first) {
            BOOST_TEST_MESSAGE("Value for " << kv.first << " with cash settlement is: " << fixed << setprecision(12)
                                            << kv.second);
            if (kv.first == "npv") {
                BOOST_CHECK_SMALL(kv.second - df_tp * valueAtExpiry, tolerance);
            } else if (kv.first == "rho") {
                BOOST_CHECK_SMALL(kv.second + tp * cashSettledResults.at("npv"), tolerance);
            } else if (kv.first == "theta") {
                if (tp > 0.0 && !close(tp, 0.0)) {
                    BOOST_CHECK_SMALL(kv.second + std::log(df_tp) / tp * cashSettledResults.at("npv"), tolerance);
                } else {
                    BOOST_CHECK(close(kv.second, 0.0));
                }
            } else if (kv.first == "thetaPerDay") {
                if (cashSettledResults.count("theta") == 0) {
                    BOOST_ERROR("Expected results to contain a value for theta");
                    continue;
                } else {
                    BOOST_CHECK_SMALL(kv.second - cashSettledResults.at("theta") / 365.0, tolerance);
                }
            } else {
                BOOST_CHECK(close(kv.second, 0.0));
            }
        }
    }
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AnalyticCashSettledEuropeanEngineTest)

vector<Real> strikes{ 55, 58, 62, 65 };
vector<Option::Type> optionTypes{ Option::Type::Call, Option::Type::Put };

BOOST_DATA_TEST_CASE(testOptionBeforeExpiry, bdata::make(strikes) * bdata::make(optionTypes), strike, optionType) {

    BOOST_TEST_MESSAGE("Testing cash settled option pricing before expiry...");

    Settings::instance().evaluationDate() = Date(3, Jun, 2020);

    // Create cash settled option instrument
    Date expiry(3, Sep, 2020);
    Date payment(7, Sep, 2020);
    bool automaticExercise = false;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Value the option accounting for cash settlement and store all results
    option.setPricingEngine(engine);
    map<string, Real> cashSettledResults = results(option);

    // Value the option ignoring cash settlement
    engine = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(getProcess(spot, vol, r, q));
    option.setPricingEngine(engine);
    map<string, Real> theoreticalResults = results(option);

    // Discount factor from payment to expiry.
    auto yts = flatYts(r);
    DiscountFactor df_te_tp = yts->discount(payment) / yts->discount(expiry);
    BOOST_TEST_MESSAGE("Discount factor from payment to expiry is: " << fixed << setprecision(12) << df_te_tp);

    // Check the results
    Real tolerance = 1e-12;
    BOOST_REQUIRE_EQUAL(cashSettledResults.size(), theoreticalResults.size());
    BOOST_REQUIRE(cashSettledResults.count("npv") == 1);
    BOOST_REQUIRE(theoreticalResults.count("npv") == 1);
    for (const auto& kv : cashSettledResults) {

        BOOST_TEST_CONTEXT("With result " << kv.first) {

            auto it = theoreticalResults.find(kv.first);
            BOOST_CHECK(it != theoreticalResults.end());

            // If there is no matching result in theoreticalResults, skip to next result.
            if (it == theoreticalResults.end())
                continue;

            // Check the result is as expected by comparing to the results that do not account for delayed payment.
            Real theorResult = it->second;
            BOOST_TEST_MESSAGE("Value for " << kv.first << " with cash settlement is: " << fixed << setprecision(12)
                                            << kv.second);
            BOOST_TEST_MESSAGE("Value for " << kv.first << " ignoring cash settlement is: " << fixed << setprecision(12)
                                            << theorResult);

            // Most results should be of the form cashSettledResult = DF(t_e, t_p) * theoreticalResult.
            // There are some exceptions dealt with below.
            if (close(theorResult, 0.0)) {
                BOOST_CHECK(close(kv.second, 0.0));
            } else if (kv.first == "elasticity" || kv.first == "itmCashProbability") {
                BOOST_CHECK(close(kv.second, theorResult));
            } else if (kv.first == "rho") {
                Time delta_te_tp = yts->timeFromReference(payment) - yts->timeFromReference(expiry);
                Real expRho = df_te_tp * (theorResult - delta_te_tp * theoreticalResults.at("npv"));
                BOOST_TEST_MESSAGE("Value for expected rho is: " << fixed << setprecision(12) << expRho);
                BOOST_CHECK(close(kv.second, expRho));
            } else {
                BOOST_CHECK_SMALL(kv.second / theorResult - df_te_tp, tolerance);
            }
        }
    }
}

BOOST_DATA_TEST_CASE(testOptionManualExerciseAfterExpiry, bdata::make(strikes) * bdata::make(optionTypes), strike,
                     optionType) {

    BOOST_TEST_MESSAGE("Testing cash settled manual exercise option pricing after expiry...");

    Settings::instance().evaluationDate() = Date(4, Sep, 2020);

    // Create cash settled option instrument
    Date expiry(3, Sep, 2020);
    Date payment(7, Sep, 2020);
    bool automaticExercise = false;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Value the option accounting for cash settlement and store all results
    option.setPricingEngine(engine);
    map<string, Real> cashSettledResults = results(option);

    // Option has not been manually exercised so all results should be zero.
    for (const auto& kv : cashSettledResults) {
        BOOST_TEST_CONTEXT("With result " << kv.first) { BOOST_CHECK(close(kv.second, 0.0)); }
    }

    // Manually exercise the option with an expiry value of 59.
    Real exercisePrice = 59.00;
    option.exercise(exercisePrice);

    // Check the updated results
    checkOptionValues(option, r, exercisePrice);
}

// Values for includeReferenceDateEvents in tests.
vector<bool> irdes{ true, false };

BOOST_DATA_TEST_CASE(testOptionManualExerciseOnExpiry,
                     bdata::make(strikes) * bdata::make(optionTypes) * bdata::make(irdes), strike, optionType, irde) {

    BOOST_TEST_MESSAGE("Testing cash settled manual exercise option on expiry date...");

    // Should work for either setting of includeReferenceDateEvents.
    Settings::instance().includeReferenceDateEvents() = irde;

    // Create cash settled option instrument
    Date expiry(3, Sep, 2020);
    Settings::instance().evaluationDate() = expiry;
    Date payment(7, Sep, 2020);
    bool automaticExercise = false;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Set the pricing engine
    option.setPricingEngine(engine);

    // We are on the expiry date but have not exercised the option. Expect the valuation to proceed and hence
    // the value to be based off the market spot price.
    checkOptionValues(option, r, spot);

    // Manually exercise the option with an expiry value of 59.
    Real exercisePrice = 59.00;
    option.exercise(exercisePrice);

    // Check the updated option values.
    checkOptionValues(option, r, exercisePrice);
}

BOOST_DATA_TEST_CASE(testOptionManualExerciseOnPayment, bdata::make(strikes) * bdata::make(optionTypes), strike,
                     optionType) {

    BOOST_TEST_MESSAGE("Testing cash settled manual exercise option on payment date...");

    // Create cash settled option instrument
    Date expiry(3, Sep, 2020);
    Date payment(7, Sep, 2020);
    Settings::instance().evaluationDate() = payment;
    bool automaticExercise = false;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Set the pricing engine
    option.setPricingEngine(engine);

    // Manually exercise the option with an expiry value of 59.
    Real exercisePrice = 59.00;
    option.exercise(exercisePrice);

    // Set include reference date events to true so that the option is not considered expired.
    Settings::instance().includeReferenceDateEvents() = true;

    // Check the option values against expected values.
    checkOptionValues(option, r, exercisePrice);

    // Set include reference date events to false so that the option is considered expired.
    // Must recalculate for the setting to take effect.
    Settings::instance().includeReferenceDateEvents() = false;
    option.recalculate();

    // Check that all the values are zero, i.e. the expired state.
    map<string, Real> cashSettledResults = results(option);
    for (const auto& kv : cashSettledResults) {
        BOOST_TEST_CONTEXT("With result " << kv.first) { BOOST_CHECK(close(kv.second, 0.0)); }
    }
}

BOOST_DATA_TEST_CASE(testOptionAutomaticExerciseAfterExpiry, bdata::make(strikes) * bdata::make(optionTypes), strike,
                     optionType) {

    BOOST_TEST_MESSAGE("Testing cash settled automatic exercise option pricing after expiry...");

    Settings::instance().evaluationDate() = Date(4, Sep, 2020);

    // Create index to be used in option.
    Date expiry(3, Sep, 2020);
    NullCalendar fixingCalendar;
    QuantLib::ext::shared_ptr<Index> index =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("TEST", expiry, fixingCalendar, priceTs());

    // Add the expiry date fixing for the index.
    Real exercisePrice = 59.00;
    index->addFixing(expiry, exercisePrice);

    // Create cash settled option instrument
    Date payment(7, Sep, 2020);
    bool automaticExercise = true;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise, index);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Set the pricing engine
    option.setPricingEngine(engine);

    // Check the option values against expected values.
    checkOptionValues(option, r, exercisePrice);
}

BOOST_DATA_TEST_CASE(testOptionAutomaticExerciseOnExpiry,
                     bdata::make(strikes) * bdata::make(optionTypes) * bdata::make(irdes), strike, optionType, irde) {

    BOOST_TEST_MESSAGE("Testing cash settled automatic exercise option pricing on expiry...");

    // Should work for either setting of includeReferenceDateEvents.
    Settings::instance().includeReferenceDateEvents() = irde;

    // Create index to be used in option.
    Date expiry(3, Sep, 2020);
    Settings::instance().evaluationDate() = expiry;
    NullCalendar fixingCalendar;
    Handle<PriceTermStructure> pts = priceTs();
    QuantLib::ext::shared_ptr<Index> index = QuantLib::ext::make_shared<CommodityFuturesIndex>("TEST", expiry, fixingCalendar, pts);

    // Create cash settled option instrument
    Date payment(7, Sep, 2020);
    bool automaticExercise = true;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise, index);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Set the pricing engine
    option.setPricingEngine(engine);

    // We have not added a fixing for the index so it will be projected of the price term structure above to
    // give the payoff. So, we use that value here in our check initially.
    Real ptsPrice = pts->price(0.0);
    checkOptionValues(option, r, ptsPrice);

    // Add an expiry date fixing for the index.
    Real exercisePrice = 59.00;
    index->addFixing(expiry, exercisePrice);

    // Check the updated values
    checkOptionValues(option, r, exercisePrice);
}

BOOST_DATA_TEST_CASE(testOptionAutomaticExerciseOnPayment, bdata::make(strikes) * bdata::make(optionTypes), strike,
                     optionType) {

    BOOST_TEST_MESSAGE("Testing cash settled automatic exercise option pricing on payment date...");

    // Create index to be used in option.
    Date expiry(3, Sep, 2020);
    NullCalendar fixingCalendar;
    QuantLib::ext::shared_ptr<Index> index =
        QuantLib::ext::make_shared<CommodityFuturesIndex>("TEST", expiry, fixingCalendar, priceTs());

    // Add the expiry date fixing for the index.
    Real exercisePrice = 59.00;
    index->addFixing(expiry, exercisePrice);

    // Create cash settled option instrument
    Date payment(7, Sep, 2020);
    Settings::instance().evaluationDate() = payment;
    bool automaticExercise = true;
    CashSettledEuropeanOption option(optionType, strike, expiry, payment, automaticExercise, index);

    // Create engine that accounts for cash settlement.
    Rate spot = 60.00;
    Volatility vol = 0.30;
    Rate r = 0.02;
    Rate q = 0.01;
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<AnalyticCashSettledEuropeanEngine>(getProcess(spot, vol, r, q));

    // Set the pricing engine
    option.setPricingEngine(engine);

    // Set include reference date events to true so that the option is not considered expired.
    Settings::instance().includeReferenceDateEvents() = true;

    // Check the option values against expected values.
    checkOptionValues(option, r, exercisePrice);

    // Set include reference date events to false so that the option is considered expired.
    // Must recalculate for the setting to take effect.
    Settings::instance().includeReferenceDateEvents() = false;
    option.recalculate();

    // Check that all the values are zero, i.e. the expired state.
    map<string, Real> cashSettledResults = results(option);
    for (const auto& kv : cashSettledResults) {
        BOOST_TEST_CONTEXT("With result " << kv.first) { BOOST_CHECK(close(kv.second, 0.0)); }
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
