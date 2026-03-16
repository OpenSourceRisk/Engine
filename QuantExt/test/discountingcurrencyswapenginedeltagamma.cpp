/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

#include <qle/pricingengines/discountingcurrencyswapengine.hpp>
#include <qle/pricingengines/discountingcurrencyswapenginedeltagamma.hpp>

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/math/matrix.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/calendars/unitedstates.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

namespace {
struct TestData {
    TestData() : refDate(Date(22, Aug, 2016)) {
        Settings::instance().evaluationDate() = refDate;
        baseDiscount = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), Actual365Fixed()));
        baseForward = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));
        baseDiscountFor = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.04)), Actual365Fixed()));
        baseForwardFor = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            refDate, Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.05)), Actual365Fixed()));
        fxQuote = QuantLib::ext::make_shared<SimpleQuote>(0.90);
        pillarDates.push_back(refDate + 1 * Years);
        pillarDates.push_back(refDate + 2 * Years);
        pillarDates.push_back(refDate + 3 * Years);
        pillarDates.push_back(refDate + 4 * Years);
        pillarDates.push_back(refDate + 5 * Years);
        pillarDates.push_back(refDate + 7 * Years);
        pillarDates.push_back(refDate + 10 * Years);
        std::vector<Handle<Quote>> tmpDiscSpr, tmpFwdSpr, tmpDiscSprFor, tmpFwdSprFor;
        for (Size i = 0; i < pillarDates.size(); ++i) {
            QuantLib::ext::shared_ptr<SimpleQuote> qd = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            QuantLib::ext::shared_ptr<SimpleQuote> qf = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            QuantLib::ext::shared_ptr<SimpleQuote> qdf = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            QuantLib::ext::shared_ptr<SimpleQuote> qff = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            discountSpreads.push_back(qd);
            forwardSpreads.push_back(qf);
            discountSpreadsFor.push_back(qdf);
            forwardSpreadsFor.push_back(qff);
            tmpDiscSpr.push_back(Handle<Quote>(qd));
            tmpFwdSpr.push_back(Handle<Quote>(qf));
            tmpDiscSprFor.push_back(Handle<Quote>(qdf));
            tmpFwdSprFor.push_back(Handle<Quote>(qff));
            pillarTimes.push_back(baseDiscount->timeFromReference(pillarDates[i]));
        }
        discountCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseDiscount, tmpDiscSpr, pillarDates));
        forwardCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseForward, tmpFwdSpr, pillarDates));
        discountCurveFor =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseDiscountFor, tmpDiscSprFor, pillarDates));
        forwardCurveFor =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseForwardFor, tmpFwdSprFor, pillarDates));
        discountCurve->enableExtrapolation();
        forwardCurve->enableExtrapolation();
        discountCurveFor->enableExtrapolation();
        forwardCurveFor->enableExtrapolation();
        forwardIndex = QuantLib::ext::make_shared<Euribor>(6 * Months, forwardCurve);
        forwardIndexFor = QuantLib::ext::make_shared<USDLibor>(3 * Months, forwardCurveFor);
    }
    Date refDate;
    Handle<YieldTermStructure> baseDiscount, baseForward, discountCurve, forwardCurve;
    Handle<YieldTermStructure> baseDiscountFor, baseForwardFor, discountCurveFor, forwardCurveFor;
    QuantLib::ext::shared_ptr<IborIndex> forwardIndex, forwardIndexFor;
    QuantLib::ext::shared_ptr<SimpleQuote> fxQuote;
    std::vector<Date> pillarDates;
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> discountSpreads, forwardSpreads;
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> discountSpreadsFor, forwardSpreadsFor;
    std::vector<Real> pillarTimes;
}; // TestData

bool check(const Real reference, const Real value) {
    if (std::fabs(reference) >= 1E-4) {
        return std::fabs((reference - value) / reference) < 1E-3;
    } else {
        return std::fabs(reference - value) < 5E-5;
    }
}

void performTest(const TestData& d, const QuantLib::ext::shared_ptr<PricingEngine>& engine0,
                 const QuantLib::ext::shared_ptr<PricingEngine>& engine, const bool receive, const Real spread,
                 const std::string& config) {

    BOOST_TEST_MESSAGE("Testing npv calculation in DiscountingCurrencySwapEngineDeltaGamma against QuantExt engine ("
                       << config << ")...");

    Date settlement = d.refDate + 2;
    Schedule scheduleEur(settlement, TARGET().advance(settlement, 10 * Years), 6 * Months, TARGET(), ModifiedFollowing,
                         ModifiedFollowing, DateGeneration::Forward, false);
    Schedule scheduleUsd(settlement, UnitedStates(UnitedStates::Settlement).advance(settlement, 10 * Years), 3 * Months, UnitedStates(UnitedStates::Settlement),
                         ModifiedFollowing, ModifiedFollowing, DateGeneration::Forward, false);
    Leg eurLeg = IborLeg(scheduleEur, d.forwardIndex).withNotionals(10.0);
    Leg usdLeg = IborLeg(scheduleUsd, d.forwardIndexFor).withNotionals(10.0).withSpreads(spread);
    std::vector<Leg> legs;
    legs.push_back(eurLeg);
    legs.push_back(usdLeg);
    std::vector<Currency> currencies;
    currencies.push_back(EURCurrency());
    currencies.push_back(USDCurrency());
    std::vector<bool> payer;
    payer.push_back(receive);
    payer.push_back(!receive);
    CurrencySwap swap(legs, payer, currencies);

    std::vector<Handle<YieldTermStructure>> discountCurves;
    discountCurves.push_back(d.discountCurve);
    discountCurves.push_back(d.discountCurveFor);

    std::vector<Handle<Quote>> fx;
    fx.push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.00)));
    fx.push_back(Handle<Quote>(d.fxQuote));

    swap.setPricingEngine(engine0);
    Real npvRef = swap.NPV();

    swap.setPricingEngine(engine);
    Real npv = swap.NPV();

    const Real tol = 1E-7;
    if (std::fabs(npv - npvRef) > tol)
        BOOST_ERROR("npv (" << npv << ") is inconsistent to expected value (" << npvRef << "), difference is "
                            << npv - npvRef << ", tolerance is " << tol);

    std::vector<Real> resultDeltaDsc =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_vector>("deltaDiscount")[EURCurrency()];
    std::vector<Real> resultDeltaFwd =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_vector>("deltaForward")[EURCurrency()];
    std::vector<Real> resultDeltaDscFor =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_vector>("deltaDiscount")[USDCurrency()];
    std::vector<Real> resultDeltaFwdFor =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_vector>("deltaForward")[USDCurrency()];

    Matrix resultGamma =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_matrix>("gamma")[EURCurrency()];
    Matrix resultGammaFor =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_matrix>("gamma")[USDCurrency()];

    Real resultDeltaFxSpot =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_scalar>("deltaFxSpot")[USDCurrency()];
    Real resultFxSpot =
        swap.result<DiscountingCurrencySwapEngineDeltaGamma::result_type_scalar>("fxSpot")[USDCurrency()];

    if (std::fabs(resultFxSpot - d.fxQuote->value()) > tol)
        BOOST_ERROR("fxSpot (" << resultFxSpot << ") is inconsistent to expected value (" << d.fxQuote->value()
                               << "), difference is " << resultFxSpot - d.fxQuote->value() << ", tolerance is " << tol);

    BOOST_TEST_MESSAGE(
        "Testing delta calculation in DiscountingCurrencySwapEngineDeltaGamma against bump and revalue results ("
        << config << ")...");

    // bump and revalue

    // use QLE engine to compute reference bump and revalue results
    swap.setPricingEngine(engine0);

    const Size n = d.pillarDates.size();

    // check results for correct dimension
    if (resultDeltaDsc.size() != n)
        BOOST_ERROR("deltaDiscount (EUR) has a wrong dimension (" << resultDeltaDsc.size() << "), expected " << n);
    if (resultDeltaFwd.size() != n)
        BOOST_ERROR("deltaForward (EUR) has a wrong dimension (" << resultDeltaFwd.size() << "), expected " << n);
    if (resultDeltaDscFor.size() != n)
        BOOST_ERROR("deltaDiscount (USD) has a wrong dimension (" << resultDeltaDsc.size() << "), expected " << n);
    if (resultDeltaFwdFor.size() != n)
        BOOST_ERROR("deltaForward (USD) has a wrong dimension (" << resultDeltaFwd.size() << "), expected " << n);
    if (resultGamma.rows() != 2 * n || resultGamma.columns() != 2 * n)
        BOOST_ERROR("gamma result matrix (EUR) has wrong dimensions ("
                    << resultGamma.rows() << "x" << resultGamma.columns() << "), expected " << 2 * n << "x" << 2 * n);
    if (resultGammaFor.rows() != 2 * n || resultGamma.columns() != 2 * n)
        BOOST_ERROR("gamma result matrix (EUR) has wrong dimensions ("
                    << resultGamma.rows() << "x" << resultGamma.columns() << "), expected " << 2 * n << "x" << 2 * n);

    // delta (npv)

    const Real bump = 1E-7;
    Real npv0 = swap.NPV();
    for (Size i = 0; i < d.pillarDates.size(); ++i) {
        d.discountSpreads[i]->setValue(bump);
        Real deltaDsc = (swap.NPV() - npv0) / bump;
        d.discountSpreads[i]->setValue(0.0);
        d.forwardSpreads[i]->setValue(bump);
        Real deltaFwd = (swap.NPV() - npv0) / bump;
        d.forwardSpreads[i]->setValue(0.0);
        d.discountSpreadsFor[i]->setValue(bump);
        // pricing engine results are in original currency
        Real deltaDscFor = (swap.NPV() - npv0) / bump / d.fxQuote->value();
        d.discountSpreadsFor[i]->setValue(0.0);
        d.forwardSpreadsFor[i]->setValue(bump);
        Real deltaFwdFor = (swap.NPV() - npv0) / bump / d.fxQuote->value();
        d.forwardSpreadsFor[i]->setValue(0.0);
        if (!check(deltaDsc, resultDeltaDsc[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (discount curve, EUR) could not be verified, analytical: "
                                           << resultDeltaDsc[i] << ", bump and revalue: " << deltaDsc);
        if (!check(deltaFwd, resultDeltaFwd[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (forward curve, EUR) could not be verified, analytical: "
                                           << resultDeltaFwd[i] << ", bump and revalue: " << deltaFwd);
        if (!check(deltaDscFor, resultDeltaDscFor[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (discount curve, USD) could not be verified, analytical: "
                                           << resultDeltaDscFor[i] << ", bump and revalue: " << deltaDscFor);
        if (!check(deltaFwdFor, resultDeltaFwdFor[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (forward curve, USD) could not be verified, analytical: "
                                           << resultDeltaFwdFor[i] << ", bump and revalue: " << deltaFwdFor);
    }

    d.fxQuote->setValue(d.fxQuote->value() + bump);
    Real deltaFxSpot = (swap.NPV() - npv0) / bump;
    d.fxQuote->setValue(d.fxQuote->value() - bump);
    if (!check(deltaFxSpot, resultDeltaFxSpot))
        BOOST_ERROR("FXSpot delta could not be verified, analytical: " << resultDeltaFxSpot
                                                                       << ", bump and revalue: " << deltaFxSpot);

    // gamma

    BOOST_TEST_MESSAGE(
        "Testing gamma calculation in DiscountingCurrencySwapEngineDeltaGamma against bump and revalue results ("
        << config << ")...");

    const Real bump2 = 1E-5;
    Matrix bumpGamma(n * 2, n * 2, 0.0), bumpGammaFor(n * 2, n * 2, 0.0);

    // dsc-dsc (EUR)
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.discountSpreads[i]->setValue(bump2);
            d.discountSpreads[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.discountSpreads[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.discountSpreads[i]->setValue(0.0);
            d.discountSpreads[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.discountSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGamma[i][j] = bumpGamma[j][i] = gamma;
        }
        // j == i
        d.discountSpreads[i]->setValue(2.0 * bump2);
        Real npvpp = swap.NPV();
        d.discountSpreads[i]->setValue(bump2);
        Real npvp = swap.NPV();
        d.discountSpreads[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv0) / (bump2 * bump2);
        bumpGamma[i][i] = gamma;
    }

    // dsc-fwd (EUR)
    for (Size i = 0; i < n; ++i) {
        // j <= i
        for (Size j = 0; j < n; ++j) {
            d.discountSpreads[i]->setValue(bump2);
            d.forwardSpreads[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.forwardSpreads[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.discountSpreads[i]->setValue(0.0);
            d.forwardSpreads[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.forwardSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGamma[i][n + j] = bumpGamma[n + j][i] = gamma;
        }
    }

    // fwd-fwd (EUR)
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.forwardSpreads[i]->setValue(bump2);
            d.forwardSpreads[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.forwardSpreads[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.forwardSpreads[i]->setValue(0.0);
            d.forwardSpreads[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.forwardSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGamma[n + i][n + j] = bumpGamma[n + j][n + i] = gamma;
        }
        // j == i
        d.forwardSpreads[i]->setValue(2.0 * bump2);
        Real npvpp = swap.NPV();
        d.forwardSpreads[i]->setValue(bump2);
        Real npvp = swap.NPV();
        d.forwardSpreads[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv0) / (bump2 * bump2);
        bumpGamma[n + i][n + i] = gamma;
    }

    // dsc-dsc (USD)
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.discountSpreadsFor[i]->setValue(bump2);
            d.discountSpreadsFor[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.discountSpreadsFor[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.discountSpreadsFor[i]->setValue(0.0);
            d.discountSpreadsFor[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.discountSpreadsFor[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGammaFor[i][j] = bumpGammaFor[j][i] = gamma / d.fxQuote->value();
        }
        // j == i
        d.discountSpreadsFor[i]->setValue(2.0 * bump2);
        Real npvpp = swap.NPV();
        d.discountSpreadsFor[i]->setValue(bump2);
        Real npvp = swap.NPV();
        d.discountSpreadsFor[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv0) / (bump2 * bump2);
        bumpGammaFor[i][i] = gamma / d.fxQuote->value();
    }

    // dsc-fwd (USD)
    for (Size i = 0; i < n; ++i) {
        // j <= i
        for (Size j = 0; j < n; ++j) {
            d.discountSpreadsFor[i]->setValue(bump2);
            d.forwardSpreadsFor[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.forwardSpreadsFor[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.discountSpreadsFor[i]->setValue(0.0);
            d.forwardSpreadsFor[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.forwardSpreadsFor[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGammaFor[i][n + j] = bumpGammaFor[n + j][i] = gamma / d.fxQuote->value();
        }
    }

    // fwd-fwd (USD)
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.forwardSpreadsFor[i]->setValue(bump2);
            d.forwardSpreadsFor[j]->setValue(bump2);
            Real npvpp = swap.NPV();
            d.forwardSpreadsFor[j]->setValue(0.0);
            Real npvp0 = swap.NPV();
            d.forwardSpreadsFor[i]->setValue(0.0);
            d.forwardSpreadsFor[j]->setValue(bump2);
            Real npv0p = swap.NPV();
            d.forwardSpreadsFor[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv0) / (bump2 * bump2);
            bumpGammaFor[n + i][n + j] = bumpGammaFor[n + j][n + i] = gamma / d.fxQuote->value();
        }
        // j == i
        d.forwardSpreadsFor[i]->setValue(2.0 * bump2);
        Real npvpp = swap.NPV();
        d.forwardSpreadsFor[i]->setValue(bump2);
        Real npvp = swap.NPV();
        d.forwardSpreadsFor[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv0) / (bump2 * bump2);
        bumpGammaFor[n + i][n + i] = gamma / d.fxQuote->value();
    }

    // check gamma (EUR)
    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            if (!check(resultGamma[i][j], bumpGamma[i][j]))
                BOOST_ERROR("gamma (EUR) entry (" << i << "," << j << ") is " << resultGamma[i][j]
                                                  << ", bump and revalue result is " << bumpGamma[i][j]);
        }
    }

    // check gamma (USD)
    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            if (!check(resultGammaFor[i][j], bumpGammaFor[i][j]))
                BOOST_ERROR("gamma (USD) entry (" << i << "," << j << ") is " << resultGammaFor[i][j]
                                                  << ", bump and revalue result is " << bumpGammaFor[i][j]);
        }
    }
} // performTests

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DiscountingCurrencySwapEngineDeltaGammaTest)

BOOST_AUTO_TEST_CASE(testNpvDeltasGammas) {

    TestData d;

    std::vector<Handle<YieldTermStructure>> discountCurves;
    discountCurves.push_back(d.discountCurve);
    discountCurves.push_back(d.discountCurveFor);
    std::vector<Handle<Quote>> fx;
    fx.push_back(Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(1.00)));
    fx.push_back(Handle<Quote>(d.fxQuote));
    std::vector<Currency> currencies;
    currencies.push_back(EURCurrency());
    currencies.push_back(USDCurrency());

    QuantLib::ext::shared_ptr<PricingEngine> engine0 =
        QuantLib::ext::make_shared<DiscountingCurrencySwapEngine>(discountCurves, fx, currencies, EURCurrency());
    QuantLib::ext::shared_ptr<PricingEngine> engine = QuantLib::ext::make_shared<DiscountingCurrencySwapEngineDeltaGamma>(
        discountCurves, fx, currencies, EURCurrency(), d.pillarTimes, true, true);

    performTest(d, engine0, engine, false, 0.0, "payer, zero spread");
    performTest(d, engine0, engine, true, 0.0, "receiver, zero spread");
    performTest(d, engine0, engine, false, 0.01, "payer, positive spread");
    performTest(d, engine0, engine, true, 0.01, "receiver, positive spread");
    performTest(d, engine0, engine, false, -0.01, "payer, negative spread");
    performTest(d, engine0, engine, true, -0.01, "receiver, negative spread");

    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
