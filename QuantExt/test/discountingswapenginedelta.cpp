/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#include "discountingswapenginedelta.hpp"

#include <qle/pricingengines/discountingswapenginedelta.hpp>

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/termstructures/yield/piecewisezerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

namespace testsuite {

namespace {
struct TestData {
    TestData() : refDate(Date(22, Aug, 2016)) {
        Settings::instance().evaluationDate() = refDate;
        baseDiscount = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(
            refDate, Handle<Quote>(boost::make_shared<SimpleQuote>(0.02)), Actual365Fixed()));
        baseForward = Handle<YieldTermStructure>(boost::make_shared<FlatForward>(
            refDate, Handle<Quote>(boost::make_shared<SimpleQuote>(0.03)), Actual365Fixed()));
        pillarDates.push_back(refDate + 1 * Years);
        pillarDates.push_back(refDate + 2 * Years);
        pillarDates.push_back(refDate + 3 * Years);
        pillarDates.push_back(refDate + 4 * Years);
        pillarDates.push_back(refDate + 5 * Years);
        pillarDates.push_back(refDate + 7 * Years);
        pillarDates.push_back(refDate + 10 * Years);
        std::vector<Handle<Quote> > tmpDiscSpr, tmpFwdSpr;
        for (Size i = 0; i < pillarDates.size(); ++i) {
            boost::shared_ptr<SimpleQuote> qd = boost::make_shared<SimpleQuote>(0.0);
            boost::shared_ptr<SimpleQuote> qf = boost::make_shared<SimpleQuote>(0.0);
            discountSpreads.push_back(qd);
            forwardSpreads.push_back(qf);
            tmpDiscSpr.push_back(Handle<Quote>(qd));
            tmpFwdSpr.push_back(Handle<Quote>(qf));
            pillarTimes.push_back(baseDiscount->timeFromReference(pillarDates[i]));
        }
        discountCurve =
            Handle<YieldTermStructure>(boost::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear> >(
                baseDiscount, tmpDiscSpr, pillarDates));
        forwardCurve =
            Handle<YieldTermStructure>(boost::make_shared<InterpolatedPiecewiseZeroSpreadedTermStructure<Linear> >(
                baseForward, tmpFwdSpr, pillarDates));
        discountCurve->enableExtrapolation();
        forwardCurve->enableExtrapolation();
        forwardIndex = boost::make_shared<Euribor>(6 * Months, forwardCurve);
    }
    Date refDate;
    Handle<YieldTermStructure> baseDiscount, baseForward, discountCurve, forwardCurve;
    boost::shared_ptr<IborIndex> forwardIndex;
    std::vector<Date> pillarDates;
    std::vector<boost::shared_ptr<SimpleQuote> > discountSpreads, forwardSpreads;
    std::vector<Real> pillarTimes;
}; // TestData
} // anonymous namespace

void DiscountingSwapEngineDeltaTest::testNpvDeltas() {

    BOOST_TEST_MESSAGE("Testing npv and delta calculation in DiscountingSwapEngineDelta...");

    TestData d;
    VanillaSwap swap = MakeVanillaSwap(13 * Years, d.forwardIndex, 0.04, 0 * Days).receiveFixed(false);
    boost::shared_ptr<PricingEngine> engine0 = boost::make_shared<DiscountingSwapEngine>(d.discountCurve);
    boost::shared_ptr<PricingEngine> engine =
        boost::make_shared<DiscountingSwapEngineDelta>(d.discountCurve, d.pillarTimes);

    swap.setPricingEngine(engine0);
    Real npvRef = swap.NPV();

    swap.setPricingEngine(engine);

    Real npv = swap.NPV();
    // std::map<Date, Real> deltaDiscount = swap.result<std::map<Date, Time> >("deltaDiscountRaw");
    // std::map<Date, Real> deltaForward = swap.result<std::map<Date, Time> >("deltaForwardRaw");

    const Real tol = 1E-6;
    if (std::fabs(npv - npvRef) > tol)
        BOOST_ERROR("npv (" << npv << ") is inconsistent to expected value (" << npvRef << "), difference is "
                            << npv - npvRef << ", tolerance is " << tol);

    // std::vector<Real> resultTimes = swap.result<std::vector<Real> >("deltaTimes");
    std::vector<Real> resultDeltaDsc = swap.result<std::vector<Real> >("deltaDiscount");
    std::vector<Real> resultDeltaFwd = swap.result<std::vector<Real> >("deltaForward");

    // bump and revalue

    const Real bump = 1E-7;
    Real npv0 = swap.NPV();
    for (Size i = 0; i < d.pillarDates.size(); ++i) {
        d.discountSpreads[i]->setValue(bump);
        Real deltaDsc = (swap.NPV() - npv0) / bump;
        d.discountSpreads[i]->setValue(0.0);
        d.forwardSpreads[i]->setValue(bump);
        Real deltaFwd = (swap.NPV() - npv0) / bump;
        d.forwardSpreads[i]->setValue(0.0);
        if (std::fabs(deltaDsc - resultDeltaDsc[i]) > tol)
            BOOST_ERROR("delta on pillar "
                        << d.pillarTimes[i] << " (discount curve) could not be verified, analytical: "
                        << resultDeltaDsc[i] << ", bump and revalue: " << deltaDsc
                        << ", difference: " << resultDeltaDsc[i] - deltaDsc << ", tolerance: " << tol);
        if (std::fabs(deltaFwd - resultDeltaFwd[i]) > tol)
            BOOST_ERROR("delta on pillar " << d.pillarTimes[i] << " (forward curve) could not be verified, analytical: "
                                           << resultDeltaFwd[i] << ", bump and revalue: " << deltaFwd
                                           << ", difference: " << resultDeltaFwd[i] - deltaFwd
                                           << ", tolerance: " << tol);
    }

    BOOST_CHECK(true);
}

test_suite* DiscountingSwapEngineDeltaTest::suite() {
    test_suite* suite = BOOST_TEST_SUITE("DiscountingSwapEngineDeltaTests");
    suite->add(BOOST_TEST_CASE(&DiscountingSwapEngineDeltaTest::testNpvDeltas));
    return suite;
}

} // namespace testsuite
