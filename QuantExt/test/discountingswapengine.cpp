/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2015 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include "discountingswapengine.hpp"

#include <qle/pricingengines/discountingswapengine.hpp>

#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/quotes/simplequote.hpp>

#include <boost/make_shared.hpp>

using namespace boost::unit_test_framework;

using namespace QuantLib;

void DiscountingSwapEngineTest::testVanillaSwap() {

    Date refDate(15, Feb, 2016);
    Settings::instance().evaluationDate() = refDate;
    // this is the default also, but we have to make sure
    // for the test against the QuantLib engine below
    Settings::instance().enforcesTodaysHistoricFixings() = false;

    // set up a floating reference date yield term structure ...
    boost::shared_ptr<SimpleQuote> rate = boost::make_shared<SimpleQuote>(0.02);
    Handle<Quote> rate_q(rate);
    Handle<YieldTermStructure> curve(
        boost::make_shared<FlatForward>(0, TARGET(), rate_q, Actual365Fixed()));

    // ... and an ibor index using this curve as its projection curve ...
    boost::shared_ptr<IborIndex> index =
        boost::make_shared<Euribor>(6 * Months, curve);

    // ... then create a vanilla swap with a floating leg referencing to this
    // index ...
    boost::shared_ptr<VanillaSwap> swap =
        MakeVanillaSwap(10 * Years, index, 0.03);

    // ... and two discounting engines, first the usual one ...
    boost::shared_ptr<PricingEngine> engine =
        boost::make_shared<QuantLib::DiscountingSwapEngine>(curve);

    // ... and another one that supports simulated fixings
    boost::shared_ptr<PricingEngine> engine_sim =
        boost::make_shared<QuantExt::DiscountingSwapEngine>(curve);

    // ... now do a time journey covering two years from now with
    // both the sim - engine and the usual one (where we provide
    // past fixings manually and compare the npvs.

    // we start with the sim - engine ...

    swap->setPricingEngine(engine_sim);
    std::vector<Real> npvs_sim;

    // enable simulated fixings
    SimulatedFixingsManager::instance().simulateFixings() = true;
    // only store the next 10 calendar days
    SimulatedFixingsManager::instance().horizon() = 10;
    // reset (required on each path, the global evaluation date has
    // to be the original reference date when doing this)
    SimulatedFixingsManager::instance().reset();

    for (Size i = 0; i < 2 * 365; ++i) {
        Date d = TARGET().advance(refDate, i * Days);
        Settings::instance().evaluationDate() = d;
        rate->setValue(0.02 + static_cast<Real>(i / 10000.0));
        Real npv = swap->NPV();
        npvs_sim.push_back(npv);
    }

    // now do the second run with the usual engine and
    // compare npvs

    Settings::instance().evaluationDate() = refDate;
    swap->setPricingEngine(engine);

    Real tol = 1.0E-12;

    Real lastEstimatedFixing = 0.0;
    for (Size i = 0; i < 2 * 365; ++i) {
        Date d = TARGET().advance(refDate, i * Days);
        Settings::instance().evaluationDate() = d;
        rate->setValue(0.02 + static_cast<Real>(i / 10000.0));
        Real npv = swap->NPV();
        if (std::fabs(npv - npvs_sim[i]) > tol) {
            BOOST_ERROR("swap npv on "
                        << d
                        << " different in QuantLib::DiscountingSwapEngine ("
                        << npv << ") and QuantExt::DiscountingSwapEngine ("
                        << npvs_sim[i] << ")");
        }
        if (i > 0) {
            index->addFixing(d, lastEstimatedFixing);
        } else {
            // on first date add todays fixing
            index->addFixing(d, index->fixing(d));
        }
        // calculate tommorows fixing, that will be stored on the next day
        rate->setValue(0.02 + static_cast<Real>((i + 1) / 10000.0));
        lastEstimatedFixing =
            index->fixing(TARGET().advance(refDate, (i + 1) * Days));
    }

} // test vanilla swap

test_suite *DiscountingSwapEngineTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("DiscountCurveTests");
    suite->add(BOOST_TEST_CASE(&DiscountingSwapEngineTest::testVanillaSwap));
    return suite;
}
