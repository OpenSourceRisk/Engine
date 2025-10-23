/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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
#include "utilities.hpp"

#include <boost/test/unit_test.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/hwconstantparametrization.hpp>
#include <qle/models/hwmodel.hpp>
#include <qle/models/hwpiecewiseparametrization.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/models/modelimpliedyieldtermstructure.hpp>
#include <qle/pricingengines/analytichwswaptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>
#include <qle/pricingengines/numericlgmmultilegoptionengine.hpp>

#include <ql/currencies/america.hpp>
#include <ql/exercise.hpp>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/indexes/ibor/usdlibor.hpp>
#include <ql/instruments/swaption.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(hwnfTest)

BOOST_AUTO_TEST_CASE(testPiecewiseConstructor) {

    BOOST_TEST_MESSAGE("testing hw piecewise parametrization basics ...");

    // Define constant parametrization
    Real forwardRate = 0.02;
    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.079152};
    Matrix sigma{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}};
    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));

    auto constantParams = ext::make_shared<IrHwConstantParametrization>(USDCurrency(), ts, sigma, kappa);

    // Define piecewise constant parametrization
    Array times = {5.0};
    std::vector<Array> piecewiseKappa = {{1.18575, 0.0189524, 0.0601251, 0.079152},
                                         {1.181209, 0.52398, 0.0601251, 0.122529}};
    std::vector<Matrix> piecewiseSigma = {{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}},
                                          {{-0.024242, 0.0105959, 0, 0}, {0, 0, 0.324324, 0.122529}}};

    auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(USDCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

    // Test correct inner dimensions of parameters
    BOOST_TEST(constantParams->n() == piecewiseParams->n());
    BOOST_TEST(constantParams->m() == piecewiseParams->m());

    // Test piecewise constant is selecting correct params
    BOOST_TEST(piecewiseParams->kappa(2.5) == piecewiseKappa[0]);
    BOOST_TEST(piecewiseParams->kappa(5.0) == piecewiseKappa[1]);
    BOOST_TEST(piecewiseParams->kappa(8.4) == piecewiseKappa[1]);

    BOOST_TEST(piecewiseParams->sigma_x(0.0) == piecewiseSigma[0]);
    BOOST_TEST(piecewiseParams->sigma_x(25.93) == piecewiseSigma[1]);
}

namespace {
template <typename T> void check_close(const T& a, const T& b, const double tol) {
    auto it1 = a.begin();
    auto it2 = b.begin();
    while (it1 != a.end()) {
        BOOST_CHECK_CLOSE(*it1, *it2, tol);
        it1++;
        it2++;
    }
}
}

BOOST_AUTO_TEST_CASE(testPiecewiseVsConstantParametrization) {

    BOOST_TEST_MESSAGE("testing hw piecewise parametrization vs. constant parametrization ...");

    Handle<YieldTermStructure> ts;

    Array times{1.0, 2.0, 3.0};
    Array kappa{0.01, 0.25, 0.85};
    Matrix sigma{{0.0070, 0.0080, 0.0020}, {0.0060, 0.0090, 0.0040}};

    auto pc = ext::make_shared<IrHwConstantParametrization>(USDCurrency(), ts, sigma, kappa);
    auto pw = ext::make_shared<IrHwPiecewiseParametrization>(USDCurrency(), ts, times,
                                                             std::vector<Matrix>(times.size() + 1, sigma),
                                                             std::vector<Array>(times.size() + 1, kappa));

    std::vector<double> checkTimes = {0.0, 0.5, 0.7, 1.0, 1.5, 2.0, 2.2, 2.5, 3.0, 3.5, 4.0, 10.0};

    double tol = 1E-10;

    for (auto const& t : checkTimes) {

        check_close(pc->sigma_x(t), pw->sigma_x(t), tol);
        check_close(pc->kappa(t), pw->kappa(t), tol);
        check_close(pc->y(t), pw->y(t), tol);

        for (auto const& T : checkTimes) {
            if (T < t)
                continue;
            check_close(pc->g(t, T), pw->g(t, T), tol);
        }
    }
}

BOOST_AUTO_TEST_CASE(testPiecewiseAsConstant) {

    BOOST_TEST_MESSAGE("testing hw piecewise parametrization vs constant analytic swaption pricing ...");

    Real forwardRate = 0.02;
    Array kappa = {1.18575, 0.0189524, 0.0601251, 0.079152};
    Matrix sigma{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}};
    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));

    auto constantParams = ext::make_shared<IrHwConstantParametrization>(USDCurrency(), ts, sigma, kappa);

    Array times = {};
    std::vector<Array> piecewiseKappa = {{1.18575, 0.0189524, 0.0601251, 0.079152}};
    std::vector<Matrix> piecewiseSigma = {{{-0.0122469, 0.0105959, 0, 0}, {0, 0, -0.117401, 0.122529}}};
    auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(USDCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

    // Create swaption and underlying swap
    Date today(10, July, 2025);
    UnitedStates cal(UnitedStates::Market::Settlement);
    Date startDate = cal.advance(today, 2 * Days);
    Date exerciseDate = cal.advance(startDate, 2 * Years);
    Date maturityDate = cal.advance(exerciseDate, 5 * Years);
    Handle<YieldTermStructure> curve(ts);
    auto index2 = ext::make_shared<USDLibor>(6 * Months, curve);

    Settings::instance().evaluationDate() = today;

    Schedule fixedSchedule(exerciseDate, maturityDate, 1 * Years, cal, Following, Following, DateGeneration::Forward,
                           false);
    Schedule floatSchedule(exerciseDate, maturityDate, 6 * Months, cal, Following, Following, DateGeneration::Forward,
                           false);
    auto underlying =
        ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                      floatSchedule, index2, 0.02, Actual360());

    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaptionConstant = QuantLib::ext::make_shared<Swaption>(underlying, exercise);
    auto swaptionPiecewise = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    ext::shared_ptr<HwModel> constantModel =
        ext::make_shared<HwModel>(constantParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);
    ext::shared_ptr<HwModel> piecewiseModel =
        ext::make_shared<HwModel>(piecewiseParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    ext::shared_ptr<PricingEngine> hwConstantEngine = boost::make_shared<AnalyticHwSwaptionEngine>(constantModel, ts);
    ext::shared_ptr<PricingEngine> hwPiecewiseEngine = boost::make_shared<AnalyticHwSwaptionEngine>(piecewiseModel, ts);

    swaptionConstant->setPricingEngine(hwConstantEngine);
    swaptionPiecewise->setPricingEngine(hwPiecewiseEngine);

    Real constantPrice = swaptionConstant->NPV();
    Real piecewisePrice = swaptionPiecewise->NPV();

    BOOST_TEST_MESSAGE("constant  param price " << constantPrice);
    BOOST_TEST_MESSAGE("piecewise param price " << piecewisePrice);
    BOOST_TEST(constantPrice == piecewisePrice);
}

// disabled for now, because work in progress
BOOST_AUTO_TEST_CASE(testPiecewiseConstant, *boost::unit_test::disabled()) {

    BOOST_TEST_MESSAGE("testing hw piecewise parametrization ( ... wip ... )");

    Real forwardRate = 0.02;

    Handle<YieldTermStructure> ts(
        QuantLib::ext::make_shared<FlatForward>(0, NullCalendar(), forwardRate, Actual365Fixed()));
    Array times = {3.0};
    std::vector<Array> piecewiseKappa = {{0.5, 0.10}, {0.1, 0.15}};
    std::vector<Matrix> piecewiseSigma = {{{-0.01, 0}, {0, 0.12}}, {{-0.02, 0}, {0, 0.05}}};

    auto piecewiseParams =
        ext::make_shared<IrHwPiecewiseParametrization>(USDCurrency(), ts, times, piecewiseSigma, piecewiseKappa);

    // Create swaption and underlying swap
    UnitedStates cal(UnitedStates::Market::Settlement);
    Date today(10, July, 2025);
    Date startDate = cal.advance(today, 2 * Days);
    Date exerciseDate = cal.advance(startDate, 2 * Years);
    Date maturityDate = cal.advance(exerciseDate, 5 * Years);
    Handle<YieldTermStructure> curve(ts);
    auto index2 = ext::make_shared<USDLibor>(6 * Months, curve);

    Schedule fixedSchedule(exerciseDate, maturityDate, 1 * Years, cal, Following, Following, DateGeneration::Forward,
                           false);
    Schedule floatSchedule(exerciseDate, maturityDate, 6 * Months, cal, Following, Following, DateGeneration::Forward,
                           false);
    auto underlying =
        ext::make_shared<VanillaSwap>(VanillaSwap::Payer, 1.0, fixedSchedule, 0.02, Thirty360(Thirty360::BondBasis),
                                      floatSchedule, index2, 0.02, Actual360());

    auto exercise = QuantLib::ext::make_shared<EuropeanExercise>(exerciseDate);
    auto swaption = QuantLib::ext::make_shared<Swaption>(underlying, exercise);

    ext::shared_ptr<HwModel> piecewiseModel =
        ext::make_shared<HwModel>(piecewiseParams, IrModel::Measure::BA, HwModel::Discretization::Euler, false);

    ext::shared_ptr<PricingEngine> hwPiecewiseEngine = boost::make_shared<AnalyticHwSwaptionEngine>(piecewiseModel, ts);

    swaption->setPricingEngine(hwPiecewiseEngine);
    Real price = swaption->NPV();

    BOOST_TEST_MESSAGE("Price of piecewise constant swaption: " << price);

    // TODO: continue testing here...

    BOOST_CHECK(true); // to avoid a warning during test runs
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
