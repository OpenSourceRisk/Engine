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

#include <qle/pricingengines/analyticeuropeanenginedeltagamma.hpp>

#include <ql/exercise.hpp>
#include <ql/pricingengines/vanilla/analyticeuropeanengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>
#include <ql/time/calendars/nullcalendar.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

namespace {
struct TestData {
    TestData() : refDate(Date(22, Aug, 2016)) {
        Settings::instance().evaluationDate() = refDate;
        rateDiscount = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            0, NullCalendar(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.02)), Actual365Fixed()));
        divDiscount = Handle<YieldTermStructure>(QuantLib::ext::make_shared<FlatForward>(
            0, NullCalendar(), Handle<Quote>(QuantLib::ext::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));
        pillarDates.push_back(refDate + 1 * Years);
        pillarDates.push_back(refDate + 2 * Years);
        pillarDates.push_back(refDate + 3 * Years);
        pillarDates.push_back(refDate + 4 * Years);
        pillarDates.push_back(refDate + 5 * Years);
        pillarDates.push_back(refDate + 7 * Years);
        pillarDates.push_back(refDate + 10 * Years);
        pillarDates.push_back(refDate + 15 * Years);
        pillarDates.push_back(refDate + 20 * Years);
        std::vector<Handle<Quote>> tmpRateSpr, tmpDivSpr;
        for (Size i = 0; i < pillarDates.size(); ++i) {
            QuantLib::ext::shared_ptr<SimpleQuote> qr = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            QuantLib::ext::shared_ptr<SimpleQuote> qd = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            rateSpreads.push_back(qr);
            divSpreads.push_back(qd);
            tmpRateSpr.push_back(Handle<Quote>(qr));
            tmpDivSpr.push_back(Handle<Quote>(qd));
            pillarTimes.push_back(rateDiscount->timeFromReference(pillarDates[i]));
        }
        rateCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                rateDiscount, tmpRateSpr, pillarDates));
        divCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                divDiscount, tmpDivSpr, pillarDates));
        rateCurve->enableExtrapolation();
        divCurve->enableExtrapolation();
        vol = QuantLib::ext::make_shared<SimpleQuote>(0.20);
        volTs = Handle<BlackVolTermStructure>(
            QuantLib::ext::make_shared<BlackConstantVol>(0, NullCalendar(), Handle<Quote>(vol), Actual365Fixed()));
        spot = QuantLib::ext::make_shared<SimpleQuote>(100.0);
        process = QuantLib::ext::make_shared<GeneralizedBlackScholesProcess>(Handle<Quote>(spot), divCurve, rateCurve, volTs);
    }
    Date refDate;
    Handle<YieldTermStructure> rateDiscount, divDiscount, rateCurve, divCurve;
    std::vector<Date> pillarDates;
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> rateSpreads, divSpreads;
    std::vector<Real> pillarTimes;
    QuantLib::ext::shared_ptr<SimpleQuote> vol, spot;
    Handle<BlackVolTermStructure> volTs;
    QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess> process;
}; // TestData

bool check(const Real reference, const Real value) {
    if (std::fabs(reference) >= 1E-4) {
        return std::fabs((reference - value) / reference) < 1E-3;
    } else {
        return std::fabs(reference - value) < 1E-5;
    }
}

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AnalyticEuropeanEngineDeltaGammaTest)

BOOST_AUTO_TEST_CASE(testNpvDeltasGammaVegas) {

    BOOST_TEST_MESSAGE("Testing npv calculation in AnalyticEuropeanEngineDeltaGamma against QuantLib engine...");

    TestData d;

    Size n = d.pillarTimes.size();

    Real strike = d.spot->value();
    Date expiryDate = d.refDate + 6 * Years;
    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff = QuantLib::ext::make_shared<PlainVanillaPayoff>(Option::Put, strike);
    QuantLib::ext::shared_ptr<Exercise> exercise = QuantLib::ext::make_shared<EuropeanExercise>(expiryDate);
    QuantLib::ext::shared_ptr<VanillaOption> option = QuantLib::ext::make_shared<VanillaOption>(payoff, exercise);

    QuantLib::ext::shared_ptr<PricingEngine> engine0 = QuantLib::ext::make_shared<AnalyticEuropeanEngine>(d.process);
    QuantLib::ext::shared_ptr<PricingEngine> engine1 =
        QuantLib::ext::make_shared<AnalyticEuropeanEngineDeltaGamma>(d.process, d.pillarTimes, d.pillarTimes, true, true);

    option->setPricingEngine(engine0);
    Real npv0 = option->NPV();

    option->setPricingEngine(engine1);
    Real npv = option->NPV();

    Real tol = 1E-8;
    if (std::fabs(npv0 - npv) > tol)
        BOOST_ERROR("npv (" << npv << ") can not be verified, expected " << npv0);

    // get results from sensi engine
    Real deltaSpot = option->result<Real>("deltaSpot");
    Real gammaSpot = option->result<Real>("gammaSpot");
    // Real theta = option->result<Real>("theta"); TODO in the pricer
    std::vector<Real> vega = option->result<std::vector<Real>>("vega");
    std::vector<Real> deltaRate = option->result<std::vector<Real>>("deltaRate");
    std::vector<Real> deltaDividend = option->result<std::vector<Real>>("deltaDividend");
    Matrix gamma = option->result<Matrix>("gamma");
    std::vector<Real> gammaSpotRate = option->result<std::vector<Real>>("gammaSpotRate");
    std::vector<Real> gammaSpotDiv = option->result<std::vector<Real>>("gammaSpotDiv");

    // use QL engine for bump and revalue results
    option->setPricingEngine(engine0);

    // check dimension of result vectors

    BOOST_TEST_MESSAGE("Checking additional results for correct dimensions in AnalyticEuropeanEngineDeltaGamma...");

    if (vega.size() != n)
        BOOST_ERROR("vega size (" << vega.size() << ") mismatch, expected " << n);
    if (deltaRate.size() != n)
        BOOST_ERROR("delta rate size (" << deltaRate.size() << ") mismatch, expected " << n);
    if (deltaDividend.size() != n)
        BOOST_ERROR("delta dividend size (" << deltaDividend.size() << ") mismatch, expected " << n);
    if (gamma.rows() != 2 * n || gamma.columns() != 2 * n)
        BOOST_ERROR("gamma size (" << gamma.rows() << "x" << gamma.columns() << ") mismatch, expected " << 2 * n << "x"
                                   << 2 * n);
    if (gammaSpotRate.size() != n)
        BOOST_ERROR("gamma spot rate size (" << gammaSpotRate.size() << ") mismatch, expected " << n);
    if (gammaSpotDiv.size() != n)
        BOOST_ERROR("gamma spot div size (" << gammaSpotDiv.size() << ") mismatch, expected " << n);

    // check results against bump and revalue calculations

    BOOST_TEST_MESSAGE(
        "Checking additional results against bump and revalue results in AnalyticEuropeanEngineDeltaGamma...");

    Real h1 = 1E-4;
    Real h2 = 1E-6;

    d.spot->setValue(d.spot->value() + h1);
    Real npvp = option->NPV();
    d.spot->setValue(d.spot->value() - 2.0 * h1);
    Real npvm = option->NPV();
    d.spot->setValue(d.spot->value() + h1);

    Real refDelta = (npvp - npvm) / (2.0 * h1);
    Real refGamma = (npvp - 2.0 * npv + npvm) / (h1 * h1);

    if (!check(refDelta, deltaSpot))
        BOOST_ERROR("could not verify delta (reference value=" << refDelta << ", result=" << deltaSpot
                                                               << ", difference=" << deltaSpot - refDelta << ")");
    if (!check(refGamma, gammaSpot))
        BOOST_ERROR("could not verify gamma (reference value=" << refGamma << ", result=" << gammaSpot
                                                               << ", difference=" << gammaSpot - refGamma << ")");

    // theta is a TODO in the pricer
    // Settings::instance().evaluationDate() = d.refDate + 1;
    // Real npvtp = option->NPV();
    // Real dt = Actual365Fixed().yearFraction(d.refDate, d.refDate + 1);
    // Settings::instance().evaluationDate() = d.refDate;
    // Real refTheta = (npvtp - npv) / dt;

    Real vegaSum = 0.0;
    for (Size i = 0; i < vega.size(); ++i) {
        vegaSum += vega[i];
    }

    d.vol->setValue(d.vol->value() + h2);
    Real npvvp = option->NPV();
    d.vol->setValue(d.vol->value() - h2);
    Real refVega = (npvvp - npv) / h2;

    if (!check(refVega, vegaSum))
        BOOST_ERROR("could not verify vega (reference value=" << refVega << ", result=" << vegaSum
                                                              << ", difference=" << vegaSum - refVega << ")");

    for (Size i = 0; i < n; ++i) {
        d.rateSpreads[i]->setValue(h2);
        Real refDeltaRate = (option->NPV() - npv) / h2;
        d.rateSpreads[i]->setValue(0.0);
        d.divSpreads[i]->setValue(h2);
        Real refDeltaDiv = (option->NPV() - npv) / h2;
        d.divSpreads[i]->setValue(0.0);
        if (!check(refDeltaRate, deltaRate[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i] << " (rate curve) could not be verified, analytical: "
                                           << deltaRate[i] << ", bump and revalue: " << refDeltaRate);
        if (!check(refDeltaDiv, deltaDividend[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (dividend curve) could not be verified, analytical: "
                                           << deltaDividend[i] << ", bump and revalue: " << refDeltaDiv);
    }

    Matrix refGammaRateDiv(n * 2, n * 2, 0.0);

    // rate-rate
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.rateSpreads[i]->setValue(h1);
            d.rateSpreads[j]->setValue(h1);
            Real npvpp = option->NPV();
            d.rateSpreads[j]->setValue(0.0);
            Real npvp0 = option->NPV();
            d.rateSpreads[i]->setValue(0.0);
            d.rateSpreads[j]->setValue(h1);
            Real npv0p = option->NPV();
            d.rateSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv) / (h1 * h1);
            refGammaRateDiv[i][j] = refGammaRateDiv[j][i] = gamma;
        }
        // j == i
        d.rateSpreads[i]->setValue(2.0 * h1);
        Real npvpp = option->NPV();
        d.rateSpreads[i]->setValue(h1);
        Real npvp = option->NPV();
        d.rateSpreads[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv) / (h1 * h1);
        refGammaRateDiv[i][i] = gamma;
    }

    // rate-div
    for (Size i = 0; i < n; ++i) {
        // j <= i
        for (Size j = 0; j < n; ++j) {
            d.rateSpreads[i]->setValue(h1);
            d.divSpreads[j]->setValue(h1);
            Real npvpp = option->NPV();
            d.divSpreads[j]->setValue(0.0);
            Real npvp0 = option->NPV();
            d.rateSpreads[i]->setValue(0.0);
            d.divSpreads[j]->setValue(h1);
            Real npv0p = option->NPV();
            d.divSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv) / (h1 * h1);
            refGammaRateDiv[i][n + j] = refGammaRateDiv[n + j][i] = gamma;
        }
    }

    // div-div
    for (Size i = 0; i < n; ++i) {
        // j < i
        for (Size j = 0; j < i; ++j) {
            d.divSpreads[i]->setValue(h1);
            d.divSpreads[j]->setValue(h1);
            Real npvpp = option->NPV();
            d.divSpreads[j]->setValue(0.0);
            Real npvp0 = option->NPV();
            d.divSpreads[i]->setValue(0.0);
            d.divSpreads[j]->setValue(h1);
            Real npv0p = option->NPV();
            d.divSpreads[j]->setValue(0.0);
            Real gamma = (npvpp - npvp0 - npv0p + npv) / (h1 * h1);
            refGammaRateDiv[n + i][n + j] = refGammaRateDiv[n + j][n + i] = gamma;
        }
        // j == i
        d.divSpreads[i]->setValue(2.0 * h1);
        Real npvpp = option->NPV();
        d.divSpreads[i]->setValue(h1);
        Real npvp = option->NPV();
        d.divSpreads[i]->setValue(0.0);
        Real gamma = (npvpp - 2.0 * npvp + npv0) / (h1 * h1);
        refGammaRateDiv[n + i][n + i] = gamma;
    }

    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            if (!check(refGammaRateDiv[i][j], gamma[i][j]))
                BOOST_ERROR("gamma entry (" << i << "," << j << ") is " << gamma[i][j]
                                            << ", bump and revalue result is " << refGammaRateDiv[i][j]);
        }
    }

    // spot-rate
    for (Size i = 0; i < n; ++i) {
        d.spot->setValue(d.spot->value() + h1);
        d.rateSpreads[i]->setValue(h1);
        Real npvpp = option->NPV();
        d.rateSpreads[i]->setValue(0.0);
        Real npvp0 = option->NPV();
        d.spot->setValue(d.spot->value() - h1);
        d.rateSpreads[i]->setValue(h1);
        Real npv0p = option->NPV();
        d.rateSpreads[i]->setValue(0.0);
        Real refGamma = (npvpp - npvp0 - npv0p + npv) / (h1 * h1);
        if (!check(refGamma, gammaSpotRate[i]))
            BOOST_ERROR("spot-rate gamma pillar " << i << " can not be verified, result is " << gammaSpotRate[i]
                                                  << ", bump and revalue is " << refGamma);
    }

    // spot-div
    for (Size i = 0; i < n; ++i) {
        d.spot->setValue(d.spot->value() + h1);
        d.divSpreads[i]->setValue(h1);
        Real npvpp = option->NPV();
        d.divSpreads[i]->setValue(0.0);
        Real npvp0 = option->NPV();
        d.spot->setValue(d.spot->value() - h1);
        d.divSpreads[i]->setValue(h1);
        Real npv0p = option->NPV();
        d.divSpreads[i]->setValue(0.0);
        Real refGamma = (npvpp - npvp0 - npv0p + npv) / (h1 * h1);
        if (!check(refGamma, gammaSpotDiv[i]))
            BOOST_ERROR("spot-div gamma pillar " << i << " can not be verified, result is " << gammaSpotDiv[i]
                                                 << ", bump and revalue is " << refGamma);
    }

    BOOST_CHECK(true);

} // testNpvDeltasGammaVegas

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
