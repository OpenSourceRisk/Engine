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

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AnalyticHwSwaptionEngineTest)

BOOST_AUTO_TEST_CASE(test1FAgainstLgm) {

    BOOST_TEST_MESSAGE("Testing analytic Hull-White vs. analytic LGM engine for 1F model...");

    Date referenceDate(13, October, 2025);
    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> discount(ext::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> forward(ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    auto sofrIndex = ext::make_shared<Sofr>(forward);

    Calendar calendar = sofrIndex->fixingCalendar();
    Date optionExpiry = calendar.advance(referenceDate, 5 * Years);
    Date startDate = calendar.advance(calendar.advance(referenceDate, 2 * Days), 5 * Years);
    Date maturityDate = calendar.advance(startDate, 10 * Years);
    Schedule schedule(startDate, maturityDate, 1 * Years, calendar, ModifiedFollowing, ModifiedFollowing,
                      DateGeneration::Backward, false);
    auto swap =
        ext::make_shared<OvernightIndexedSwap>(VanillaSwap::Payer, 100.0, schedule, 0.025, Actual360(), sofrIndex, 0.0);
    auto exercise = ext::make_shared<EuropeanExercise>(optionExpiry);
    auto swaption = ext::make_shared<Swaption>(swap, exercise);

    Real sigma = 0.0070;
    Real kappa = 0.01;

    auto hwModel = ext::make_shared<HwModel>(
        ext::make_shared<IrHwConstantParametrization>(USDCurrency(), discount, Matrix{{sigma}}, Array{kappa}),
        IrModel::Measure::BA, HwModel::Discretization::Euler, false);
    auto lgmModel = ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(USDCurrency(), discount, Array(),
                                                                               Array{sigma}, Array(), Array{kappa});

    auto hwEngine = ext::make_shared<AnalyticHwSwaptionEngine>(hwModel);
    auto lgmEngine = ext::make_shared<AnalyticLgmSwaptionEngine>(lgmModel, discount,
                                                                 AnalyticLgmSwaptionEngine::FloatSpreadMapping::simple);

    swaption->setPricingEngine(hwEngine);
    Real hwNpv = swaption->NPV();

    swaption->setPricingEngine(lgmEngine);
    Real lgmNpv = swaption->NPV();

    BOOST_TEST_MESSAGE("Hull-White NPV: " << hwNpv);
    BOOST_TEST_MESSAGE("LGM        NPV: " << lgmNpv);

    BOOST_CHECK_CLOSE(hwNpv, lgmNpv, 1.0);
}

BOOST_AUTO_TEST_CASE(test2FAgainstMC) {

    BOOST_TEST_MESSAGE("Testing analytic Hull-White vs. MC for 2F model...");

    Date referenceDate(13, October, 2025);
    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> discount(ext::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> forward(ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    RelinkableHandle<YieldTermStructure> indexForward(*forward);

    auto sofrIndex = ext::make_shared<Sofr>(indexForward);

    Calendar calendar = sofrIndex->fixingCalendar();
    Date optionExpiry = calendar.advance(referenceDate, 5 * Years);
    Date startDate = calendar.advance(calendar.advance(referenceDate, 2 * Days), 5 * Years);
    Date maturityDate = calendar.advance(startDate, 10 * Years);
    Schedule schedule(startDate, maturityDate, 1 * Years, calendar, ModifiedFollowing, ModifiedFollowing,
                      DateGeneration::Backward, false);
    auto swap =
        ext::make_shared<OvernightIndexedSwap>(VanillaSwap::Payer, 100.0, schedule, 0.025, Actual360(), sofrIndex, 0.0);
    auto exercise = ext::make_shared<EuropeanExercise>(optionExpiry);
    auto swaption = ext::make_shared<Swaption>(swap, exercise);

    Matrix sigma{{0.002, 0.008}, {0.009, 0.001}};
    Array kappa{0.01, 0.20};

    auto hwModel =
        ext::make_shared<HwModel>(ext::make_shared<IrHwConstantParametrization>(USDCurrency(), discount, sigma, kappa),
                                  IrModel::Measure::BA, HwModel::Discretization::Euler, true);
    auto hwEngine = ext::make_shared<AnalyticHwSwaptionEngine>(hwModel);
    swaption->setPricingEngine(hwEngine);
    Real hwNpv = swaption->NPV();

    auto process = hwModel->stateProcess();

    auto modelDiscount = ext::make_shared<ModelImpliedYtsFwdFwdCorrected>(hwModel, discount);
    auto modelForward = ext::make_shared<ModelImpliedYtsFwdFwdCorrected>(hwModel, forward);

    indexForward.linkTo(modelForward);
    swap->setPricingEngine(ext::make_shared<DiscountingSwapEngine>(Handle<YieldTermStructure>(modelDiscount)));

    Real T = discount->timeFromReference(optionExpiry);
    Size steps = 5 * 48;
    Size paths = 10000;
    TimeGrid grid(T, steps);

    MultiPathGeneratorSobolBrownianBridge pgen(process, grid, SobolBrownianGenerator::Steps, 42);

    MultiPath p;
    Array x(2);
    Array aux(2);

    boost::accumulators::accumulator_set<
        double, boost::accumulators::stats<boost::accumulators::tag::mean,
                                           boost::accumulators::tag::error_of<boost::accumulators::tag::mean>>>
        acc;

    modelDiscount->referenceDate(optionExpiry);
    modelForward->referenceDate(optionExpiry);

    for (Size i = 0; i < paths; ++i) {
        p = pgen.next().value;
        x[0] = p[0].back();
        x[1] = p[1].back();
        aux[0] = p[2].back();
        aux[1] = p[3].back();
        modelDiscount->state(x);
        modelForward->state(x);
        acc(std::max(swap->NPV(), 0.0) / hwModel->numeraire(T, x, discount, aux));
    }

    Real mcNpv = boost::accumulators::mean(acc);
    Real mcErr = boost::accumulators::error_of<boost::accumulators::tag::mean>(acc);

    BOOST_TEST_MESSAGE("Hull-White NPV: " << hwNpv);
    BOOST_TEST_MESSAGE("MC         NPV: " << mcNpv << " +- " << mcErr);

    /* Relatively large difference; when reducing the test case to 1F, analytical lgm and hw engines are consistent
       while MC and numerical lgm are consistent too and a few basispoints higher in the swaption npv.
       TODO: Review at some point. It's not specific to the HW implementation though. */

    BOOST_CHECK_CLOSE(hwNpv, mcNpv, 5.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
