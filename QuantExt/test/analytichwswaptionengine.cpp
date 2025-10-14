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

#include <qle/models/hwconstantparametrization.hpp>
#include <qle/models/hwmodel.hpp>
#include <qle/models/irlgm1fpiecewiseconstanthullwhiteadaptor.hpp>
#include <qle/pricingengines/analytichwswaptionengine.hpp>
#include <qle/pricingengines/analyticlgmswaptionengine.hpp>

#include <ql/instruments/swaption.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/indexes/ibor/sofr.hpp>
#include <ql/exercise.hpp>
#include <ql/currencies/america.hpp>

using namespace QuantLib;
using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AnalyticHwSwaptionEngineTest)

BOOST_AUTO_TEST_CASE(testOneFactorAgainstLgm) {

    BOOST_TEST_MESSAGE("Testing analytic Hull-White vs. analytic LGM engine for one factor case...");

    Date referenceDate(13, October, 2025);
    Settings::instance().evaluationDate() = referenceDate;

    Handle<YieldTermStructure> discount(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.01, Actual365Fixed()));
    Handle<YieldTermStructure> forward(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, Actual365Fixed()));

    auto sofrIndex = QuantLib::ext::make_shared<Sofr>(forward);

    Calendar calendar = sofrIndex->fixingCalendar();
    Date optionExpiry = calendar.advance(referenceDate, 5 * Years);
    Date startDate = calendar.advance(calendar.advance(referenceDate, 2 * Days), 5 * Years);
    Date maturityDate = calendar.advance(startDate, 10 * Years);
    Schedule schedule(startDate, maturityDate, 1 * Years, calendar, ModifiedFollowing, ModifiedFollowing,
                      DateGeneration::Backward, false);
    auto swap = ext::make_shared<OvernightIndexedSwap>(VanillaSwap::Payer, 100.0, schedule, 0.025, Actual365Fixed(),
                                                       sofrIndex, 0.0);
    auto exercise = ext::make_shared<EuropeanExercise>(optionExpiry);
    auto swaption = ext::make_shared<Swaption>(swap, exercise);

    Real sigma = 0.0070;
    Real kappa = 0.01;

    auto hwModel = ext::make_shared<HwModel>(
        ext::make_shared<IrHwConstantParametrization>(USDCurrency(), discount, Matrix(1, 1, sigma), Array{kappa}),
        IrModel::Measure::BA, HwModel::Discretization::Euler, false);
    auto lgmModel = QuantLib::ext::make_shared<IrLgm1fPiecewiseConstantHullWhiteAdaptor>(
        USDCurrency(), discount, Array(), Array{sigma}, Array(), Array{kappa});

    auto hwEngine = QuantLib::ext::make_shared<AnalyticHwSwaptionEngine>(hwModel);
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

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
