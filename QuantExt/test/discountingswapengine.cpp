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
#include <test-suite/utilities.hpp>

#include <boost/make_shared.hpp>
#include <boost/math/special_functions/fpclassify.hpp>

using namespace boost::unit_test_framework;

using namespace QuantLib;

void DiscountingSwapEngineTest::testVanillaSwap() {

    BOOST_TEST_MESSAGE("Testing discounting swap engine with simulated "
                       "fixings, daily steps...");

    SavedSettings backup;

    Date refDate(15, Feb, 2016);
    Settings::instance().evaluationDate() = refDate;
    // this is the default also, but we make the setting
    // explicit for this test here
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

    // set simulated fixings settings to defaults
    SimulatedFixingsManager::instance().reset();

    for (Size flavour = 0; flavour < 4; ++flavour) {

        // clear native fixing history for each run
        IndexManager::instance().clearHistory(index->name());

        // we start with the sim - engine ...

        Settings::instance().evaluationDate() = refDate;
        swap->setPricingEngine(engine_sim);
        std::vector<Real> npvs_sim;

        // enable simulated fixings
        SimulatedFixingsManager::instance().simulateFixings() = true;
        // use forward mode
        // only store the next 10 calendar days w.r.t. forward fixings
        SimulatedFixingsManager::instance().horizon() = 10;

        // set the estimation method
        if (flavour == 0)
            SimulatedFixingsManager::instance().estimationMethod() =
                SimulatedFixingsManager::Forward;
        if (flavour == 1)
            SimulatedFixingsManager::instance().estimationMethod() =
                SimulatedFixingsManager::Backward;
        if (flavour == 2)
            SimulatedFixingsManager::instance().estimationMethod() =
                SimulatedFixingsManager::BestOfForwardBackward;
        if (flavour == 3)
            SimulatedFixingsManager::instance().estimationMethod() =
                SimulatedFixingsManager::InterpolatedForwardBackward;

        // start new path 
        SimulatedFixingsManager::instance().newPath();

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

        Real tol = 2.0E-5; // ibor forward optimization in QLE

        for (Size i = 0; i < 2 * 365; ++i) {
            Date d = TARGET().advance(refDate, i * Days);
            Settings::instance().evaluationDate() = d;
            rate->setValue(0.02 + static_cast<Real>(i / 10000.0));
            Real npv = swap->NPV();
            if (std::fabs(npv - npvs_sim[i]) > tol ||
                boost::math::isnan(npvs_sim[i])) {
                BOOST_ERROR(
                    "swap npv on "
                    << d << " different in QuantLib::DiscountingSwapEngine ("
                    << npv << ") and QuantExt::DiscountingSwapEngine ("
                    << npvs_sim[i] << "), estimation method " << flavour);
            }
            index->addFixing(d, index->fixing(d));
        }
    }

    BOOST_CHECK(true);

} // test vanilla swap

namespace {
void checkFixing(const SimulatedFixingsManager::EstimationMethod method,
                 const Date &d, const Real expected) {
    Real tol = 1.0E-12;
    SimulatedFixingsManager::instance().estimationMethod() = method;
    Real fixing =
        SimulatedFixingsManager::instance().simulatedFixing("dummy-index", d);
    if (std::fabs(fixing - expected) > tol || boost::math::isnan(fixing)) {
        BOOST_ERROR("can not verify fixing on date "
                    << d << ", method " << method << ", expected " << expected
                    << ", actual " << fixing);
    }
} // checkFixing
} // namespace

void DiscountingSwapEngineTest::testFixingEstimationMethods() {

    BOOST_TEST_MESSAGE("Testing estimation methods for simulated fixings...");

    SavedSettings backup;

    Date refDate(15, Feb, 2016);
    Settings::instance().evaluationDate() = refDate;
    // see above, only to have an explicit setting in the test
    Settings::instance().enforcesTodaysHistoricFixings() = false;

    // enable simulated fixings and start new path
    SimulatedFixingsManager::instance().reset();
    SimulatedFixingsManager::instance().simulateFixings() = true;
    SimulatedFixingsManager::instance().newPath();

    // move forward, add a backward fixing and test retrieving a past fixing
    // fwd should return null
    // bwd should return the value added
    // the combined methods should fall back on the bwd method

    Settings::instance().evaluationDate() = Date(15, Aug, 2016);

    SimulatedFixingsManager::instance().addBackwardFixing("dummy-index", 0.03);

    checkFixing(SimulatedFixingsManager::Forward, Date(15, Jul, 2016),
                Null<Real>());
    checkFixing(SimulatedFixingsManager::Backward, Date(15, Jul, 2016),
    0.03);
    checkFixing(SimulatedFixingsManager::BestOfForwardBackward,
                Date(15, Jul, 2016), 0.03);
    checkFixing(SimulatedFixingsManager::InterpolatedForwardBackward,
                Date(15, Jul, 2016), 0.03);

    // add a 1y-forward fixing and move 1m behind this and retrieve it
    // fwd should return the added value
    // bwd should return null
    // the combined methods should fall back on the fwd method

    SimulatedFixingsManager::instance().addForwardFixing(
        "dummy-index", Date(15, Aug, 2017), 0.05);

    Settings::instance().evaluationDate() = Date(15, Sep, 2017);
    checkFixing(SimulatedFixingsManager::Forward, Date(15, Aug, 2017), 0.05);
    checkFixing(SimulatedFixingsManager::Backward, Date(15, Aug, 2017),
                Null<Real>());
    checkFixing(SimulatedFixingsManager::BestOfForwardBackward,
                Date(15, Aug, 2017), 0.05);
    checkFixing(SimulatedFixingsManager::InterpolatedForwardBackward,
                Date(15, Aug, 2017), 0.05);

    // now add a backward fixing:
    // fwd should return 0.05 still
    // bwd should return 0.06
    // best of should prefer bwd
    // interpolated should return roughly (1/12*fwd+1*bwd)/(1/12+1)

    // for checking the interpolated value
    BigNatural bwddays = Date(15, Sep, 2017) - Date(15, Aug, 2017);
    BigNatural fwddays = Date(15, Aug, 2017) - Date(15, Aug, 2016);

    SimulatedFixingsManager::instance().addBackwardFixing("dummy-index", 0.06);
    checkFixing(SimulatedFixingsManager::Forward, Date(15, Aug, 2017), 0.05);
    checkFixing(SimulatedFixingsManager::Backward, Date(15, Aug, 2017),
                0.06);
    checkFixing(SimulatedFixingsManager::BestOfForwardBackward,
                Date(15, Aug, 2017), 0.06);
    checkFixing(SimulatedFixingsManager::InterpolatedForwardBackward,
                Date(15, Aug, 2017),
                (bwddays*0.05+fwddays*0.06)/(bwddays+fwddays));

    // add a 1y-forward again, a bwd too, then add a 1m-forward for the same
    // fixing date,
    // a bwd again, and move 1y behind it
    // fwd should give the last added fixing 0.08
    // bwd should give null
    // combined methods should give fwd
    SimulatedFixingsManager::instance().addForwardFixing(
        "dummy-index", Date(15, Sep, 2018), 0.07);
    SimulatedFixingsManager::instance().addBackwardFixing("dummy-index", 0.071);
    Settings::instance().evaluationDate() = Date(15, Aug, 2018);
    SimulatedFixingsManager::instance().addForwardFixing(
        "dummy-index", Date(15, Sep, 2018), 0.08);
    SimulatedFixingsManager::instance().addBackwardFixing("dummy-index", 0.081);
    Settings::instance().evaluationDate() = Date(15, Sep, 2019);
    checkFixing(SimulatedFixingsManager::Forward, Date(15, Sep, 2018), 0.08);
    checkFixing(SimulatedFixingsManager::Backward, Date(15, Sep, 2018),
                Null<Real>());
    checkFixing(SimulatedFixingsManager::BestOfForwardBackward,
                Date(15, Sep, 2018), 0.08);
    checkFixing(SimulatedFixingsManager::InterpolatedForwardBackward,
                Date(15, Sep, 2018), 0.08);

    // finally add a backward fixing and check again
    // fwd should still give 0.08
    // bwd should now give 0.09
    // best of should prefer the fwd now, since it is closer
    // interpolated should roughly give (1*fwd+1/12*bwd)/(1+1/12)
    bwddays = Date(15, Sep, 2019) - Date(15, Sep, 2018);
    fwddays = Date(15, Sep, 2018) - Date(15, Aug, 2018);
    SimulatedFixingsManager::instance().addBackwardFixing("dummy-index", 0.09);
    checkFixing(SimulatedFixingsManager::Forward, Date(15, Sep, 2018), 0.08);
    checkFixing(SimulatedFixingsManager::Backward, Date(15, Sep, 2018), 0.09);
    checkFixing(SimulatedFixingsManager::BestOfForwardBackward,
                Date(15, Sep, 2018), 0.08);
    checkFixing(SimulatedFixingsManager::InterpolatedForwardBackward,
                Date(15, Sep, 2018),
                (bwddays * 0.08 + fwddays * 0.09) / (bwddays + fwddays));

    BOOST_CHECK(true);

} // testFixingsEstimationMethods

test_suite *DiscountingSwapEngineTest::suite() {
    test_suite *suite = BOOST_TEST_SUITE("DiscountingSwapEngineTests");
    suite->add(QUANTLIB_TEST_CASE(&DiscountingSwapEngineTest::testVanillaSwap));
    suite->add(QUANTLIB_TEST_CASE(
        &DiscountingSwapEngineTest::testFixingEstimationMethods));
    return suite;
}
