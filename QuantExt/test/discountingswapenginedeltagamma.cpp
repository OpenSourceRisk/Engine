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

#include <qle/pricingengines/discountingswapenginedeltagamma.hpp>

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>
#include <boost/timer/timer.hpp>

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
        pillarDates.push_back(refDate + 1 * Years);
        pillarDates.push_back(refDate + 2 * Years);
        pillarDates.push_back(refDate + 3 * Years);
        pillarDates.push_back(refDate + 4 * Years);
        pillarDates.push_back(refDate + 5 * Years);
        pillarDates.push_back(refDate + 7 * Years);
        pillarDates.push_back(refDate + 10 * Years);
        std::vector<Handle<Quote>> tmpDiscSpr, tmpFwdSpr;
        for (Size i = 0; i < pillarDates.size(); ++i) {
            QuantLib::ext::shared_ptr<SimpleQuote> qd = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            QuantLib::ext::shared_ptr<SimpleQuote> qf = QuantLib::ext::make_shared<SimpleQuote>(0.0);
            discountSpreads.push_back(qd);
            forwardSpreads.push_back(qf);
            tmpDiscSpr.push_back(Handle<Quote>(qd));
            tmpFwdSpr.push_back(Handle<Quote>(qf));
            pillarTimes.push_back(baseDiscount->timeFromReference(pillarDates[i]));
        }
        discountCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseDiscount, tmpDiscSpr, pillarDates));
        forwardCurve =
            Handle<YieldTermStructure>(QuantLib::ext::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear>>(
                baseForward, tmpFwdSpr, pillarDates));
        discountCurve->enableExtrapolation();
        forwardCurve->enableExtrapolation();
        forwardIndex = QuantLib::ext::make_shared<Euribor>(6 * Months, forwardCurve);
    }
    Date refDate;
    Handle<YieldTermStructure> baseDiscount, baseForward, discountCurve, forwardCurve;
    QuantLib::ext::shared_ptr<IborIndex> forwardIndex;
    std::vector<Date> pillarDates;
    std::vector<QuantLib::ext::shared_ptr<SimpleQuote>> discountSpreads, forwardSpreads;
    std::vector<Real> pillarTimes;
}; // TestData

bool check(const Real reference, const Real value) {
    if (std::fabs(reference) >= 1E-4) {
        return std::fabs((reference - value) / reference) < 1E-3;
    } else {
        return std::fabs(reference - value) < 5E-5;
    }
}

class Timer {
    boost::timer::cpu_timer timer_;
    double elapsed_;

public:
    void start() { timer_ = boost::timer::cpu_timer(); }
    void stop() { elapsed_ = timer_.elapsed().wall * 1e-9; }
    double elapsed() const { return elapsed_; }
};

void performTest(const TestData& d, const QuantLib::ext::shared_ptr<PricingEngine>& engine0,
                 const QuantLib::ext::shared_ptr<PricingEngine>& engine, const bool receiveFixed, const Real spread,
                 const std::string& config) {
    BOOST_TEST_MESSAGE("Testing npv and bps calculation in DiscountingSwapEngineDeltaGamma against QuantLib engine ("
                       << config << ")...");

    VanillaSwap swap = MakeVanillaSwap(13 * Years, d.forwardIndex, 0.04, 0 * Days)
                           .receiveFixed(receiveFixed)
                           .withNominal(10.0)
                           .withFloatingLegSpread(spread);

    swap.setPricingEngine(engine0);
    Timer timer;
    timer.start();
    Real npvRef = swap.NPV();
    timer.stop();
    // BOOST_TEST_MESSAGE("ql engine took " << timer.elapsed() * 1E6 << " mus");
    Real bps1Ref = swap.legBPS(0) * 10000.0;
    Real bps2Ref = swap.legBPS(1) * 10000.0;

    swap.setPricingEngine(engine);
    timer.start();
    Real npv = swap.NPV();
    timer.stop();
    // BOOST_TEST_MESSAGE("dg engine took " << timer.elapsed() * 1E6 << " mus");
    Real bps1 = swap.legBPS(0);
    Real bps2 = swap.legBPS(1);

    const Real tol0 = 1E-10;
    if (std::fabs(npv - npvRef) > tol0)
        BOOST_ERROR("npv (" << npv << ") is inconsistent to expected value (" << npvRef << "), difference is "
                            << npv - npvRef << ", tolerance is " << tol0);
    if (std::fabs(bps1 - bps1Ref) > tol0)
        BOOST_ERROR("bps leg #1 (" << bps1 << ") is inconsistent to expected value (" << bps1Ref << "), difference is "
                                   << bps1 - bps1Ref << ", tolerance is " << tol0);
    if (std::fabs(bps2 - bps2Ref) > tol0)
        BOOST_ERROR("bps leg #2 (" << bps2 << ") is inconsistent to expected value (" << bps2Ref << "), difference is "
                                   << bps2 - bps2Ref << ", tolerance is " << tol0);

    BOOST_TEST_MESSAGE("Testing delta calculation in DiscountingSwapEngineDeltaGamma against bump and revalue results ("
                       << config << ")...");

    std::vector<Real> resultDeltaDsc = swap.result<std::vector<Real>>("deltaDiscount");
    std::vector<Real> resultDeltaFwd = swap.result<std::vector<Real>>("deltaForward");
    Matrix resultGamma = swap.result<Matrix>("gamma");
    std::vector<std::vector<Real>> resultDeltaBPS = swap.result<std::vector<std::vector<Real>>>("deltaBPS");
    std::vector<Matrix> resultGammaBPS = swap.result<std::vector<Matrix>>("gammaBPS");

    // bump and revalue checks

    // use QL engine to compute reference bump and revalue results
    swap.setPricingEngine(engine0);

    const Size n = d.pillarDates.size();

    // check results for correct dimensions
    if (resultDeltaDsc.size() != n)
        BOOST_ERROR("deltaDiscount result vector has a wrong dimension (" << resultDeltaDsc.size() << "), expected "
                                                                          << n);
    if (resultDeltaFwd.size() != n)
        BOOST_ERROR("deltaForward result vector has a wrong dimension (" << resultDeltaFwd.size() << "), expected "
                                                                         << n);
    for (Size l = 0; l < 2; ++l) {
        if (resultDeltaBPS[l].size() != n)
            BOOST_ERROR("deltaBPS result vector for leg #" << l << " has a wrong dimension (" << resultDeltaBPS.size()
                                                           << "), expected " << n);
        if (resultGammaBPS[l].rows() != n || resultGammaBPS[l].columns() != n)
            BOOST_ERROR("gamma result matrix has wrong dimensions ("
                        << resultGammaBPS[l].rows() << "x" << resultGammaBPS[l].columns() << "), expected " << 2 * n
                        << "x" << 2 * n);
    }

    if (resultGamma.rows() != 2 * n || resultGamma.columns() != 2 * n)
        BOOST_ERROR("gamma result matrix has wrong dimensions (" << resultGamma.rows() << "x" << resultGamma.columns()
                                                                 << "), expected " << 2 * n << "x" << 2 * n);

    // delta (npv)

    const Real bump = 1E-7;
    Real npv0 = swap.NPV();
    std::vector<Real> legBPS0;
    legBPS0.push_back(swap.legBPS(0) * 1E4);
    legBPS0.push_back(swap.legBPS(1) * 1E4);

    for (Size i = 0; i < n; ++i) {
        d.discountSpreads[i]->setValue(bump);
        Real deltaDsc = (swap.NPV() - npv0) / bump;
        d.discountSpreads[i]->setValue(0.0);
        d.forwardSpreads[i]->setValue(bump);
        Real deltaFwd = (swap.NPV() - npv0) / bump;
        d.forwardSpreads[i]->setValue(0.0);
        if (!check(deltaDsc, resultDeltaDsc[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i]
                                           << " (discount curve) could not be verified, analytical: "
                                           << resultDeltaDsc[i] << ", bump and revalue: " << deltaDsc);
        if (!check(deltaFwd, resultDeltaFwd[i]))
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i] << " (forward curve) could not be verified, analytical: "
                                           << resultDeltaFwd[i] << ", bump and revalue: " << deltaFwd);
    }

    // delta (bps, per leg)

    for (Size l = 0; l < 2; ++l) {
        for (Size i = 0; i < n; ++i) {
            d.discountSpreads[i]->setValue(bump);
            Real deltaDsc = (swap.legBPS(l) * 1E4 - legBPS0[l]) / bump;
            d.discountSpreads[i]->setValue(0.0);
            if (!check(deltaDsc / 1E4, resultDeltaBPS[l][i] / 1E4))
                BOOST_ERROR("bps-delta for leg #" << (l + 1) << " on pillar " << d.pillarTimes[i]
                                                  << " (discount curve) could not be verified, analytical: "
                                                  << resultDeltaBPS[l][i] << ", bump and revalue: " << deltaDsc);
        }
    }

    // gamma (npv)

    const Real bump2 = 1E-5;
    Matrix bumpGamma(n * 2, n * 2, 0.0);

    // dsc-dsc
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

    // dsc-fwd
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

    // fwd-fwd
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

    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            if (!check(bumpGamma[i][j], resultGamma[i][j]))
                BOOST_ERROR("gamma entry (" << i << "," << j << ") is " << resultGamma[i][j]
                                            << ", bump and revalue result is " << bumpGamma[i][j]);
        }
    }

    // gamma (bps, per leg)

    for (Size l = 0; l < 2; ++l) {
        Matrix bumpGammaBPS(n, n, 0.0);
        for (Size i = 0; i < n; ++i) {
            // j < i
            for (Size j = 0; j < i; ++j) {
                d.discountSpreads[i]->setValue(bump2);
                d.discountSpreads[j]->setValue(bump2);
                Real bpspp = swap.legBPS(l) * 1E4;
                d.discountSpreads[j]->setValue(0.0);
                Real bpsp0 = swap.legBPS(l) * 1E4;
                d.discountSpreads[i]->setValue(0.0);
                d.discountSpreads[j]->setValue(bump2);
                Real bps0p = swap.legBPS(l) * 1E4;
                d.discountSpreads[j]->setValue(0.0);
                Real gamma = (bpspp - bpsp0 - bps0p + legBPS0[l]) / (bump2 * bump2);
                bumpGammaBPS[i][j] = bumpGammaBPS[j][i] = gamma;
            }
            // j == i
            d.discountSpreads[i]->setValue(2.0 * bump2);
            Real bpspp = swap.legBPS(l) * 1E4;
            d.discountSpreads[i]->setValue(bump2);
            Real bpsp = swap.legBPS(l) * 1E4;
            d.discountSpreads[i]->setValue(0.0);
            Real gamma = (bpspp - 2.0 * bpsp + legBPS0[l]) / (bump2 * bump2);
            bumpGammaBPS[i][i] = gamma;
        }
        for (Size i = 0; i < n; ++i) {
            for (Size j = 0; j < n; ++j) {
                if (!check(bumpGammaBPS[i][j] / 1E4, resultGammaBPS[l][i][j] / 1E4))
                    BOOST_ERROR("bps-gamma for leg #"
                                << (l + 1) << " at (" << i << "," << j << ")"
                                << " could not be verified, analytical: " << resultGammaBPS[l][i][j]
                                << ", bump and revalue: " << bumpGammaBPS[i][j]);
            }
        }
    }

    BOOST_TEST_MESSAGE(
        "Testing sum of deltas and gammas in DiscountingSwapEngineDeltaGamma against parallel bump of all yields ("
        << config << ")...");

    // totals (parallel shift over all curves)
    // this tests, if we have identified all non-zero first and second order partial derivatives

    for (Size i = 0; i < n; ++i) {
        d.discountSpreads[i]->setValue(bump);
        d.forwardSpreads[i]->setValue(bump);
    }

    Real totalDeltaBump = (swap.NPV() - npv0) / bump;

    for (Size i = 0; i < n; ++i) {
        d.discountSpreads[i]->setValue(2.0 * bump2);
        d.forwardSpreads[i]->setValue(2.0 * bump2);
    }
    Real npvPP = swap.NPV();
    for (Size i = 0; i < n; ++i) {
        d.discountSpreads[i]->setValue(bump2);
        d.forwardSpreads[i]->setValue(bump2);
    }
    Real npvP = swap.NPV();
    for (Size i = 0; i < n; ++i) {
        d.discountSpreads[i]->setValue(0.0);
        d.forwardSpreads[i]->setValue(0.0);
    }

    Real totalGammaBump = (npvPP - 2.0 * npvP + npv0) / (bump2 * bump2);

    Real totalDelta = 0.0, totalGamma = 0.0;
    for (Size i = 0; i < n; ++i) {
        totalDelta += resultDeltaDsc[i];
        totalDelta += resultDeltaFwd[i];
    }

    for (Size i = 0; i < 2 * n; ++i) {
        for (Size j = 0; j < 2 * n; ++j) {
            totalGamma += resultGamma[i][j];
        }
    }

    if (!check(totalDeltaBump, totalDelta))
        BOOST_ERROR("total delta (" << totalDelta << ") can not be verified against bump and revalue result ("
                                    << totalDeltaBump << ")");

    if (!check(totalGammaBump, totalGamma))
        BOOST_ERROR("total gamma (" << totalGamma << ") can not be verified against bump and revalue result ("
                                    << totalGammaBump << ")");

} // performTest

} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DiscountingSwapEngineDeltaGammaTest)

BOOST_AUTO_TEST_CASE(testNpvDeltasGammas) {

    TestData d;

    QuantLib::ext::shared_ptr<PricingEngine> engine0 = QuantLib::ext::make_shared<DiscountingSwapEngine>(d.discountCurve);
    QuantLib::ext::shared_ptr<PricingEngine> engine =
        QuantLib::ext::make_shared<DiscountingSwapEngineDeltaGamma>(d.discountCurve, d.pillarTimes, true, true, true);

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
