/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include "utilities.hpp"
#include "toplevelfixture.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/make_shared.hpp>
#include <boost/test/data/test_case.hpp>

#include <ql/currencies/america.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/thirty360.hpp>

#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/commodityschwartzmodel.hpp>
#include <qle/models/commodityschwartzparametrization.hpp>
#include <qle/processes/commodityschwartzstateprocess.hpp>
#include <qle/termstructures/pricecurve.hpp>
//#include <qle/models/commodityschwartzseasonalitymodel.hpp>
//#include <qle/models/commodityschwartzseasonalityparametrization.hpp>

#include <fstream>
#include <iostream>

// fix for boost 1.64, see https://lists.boost.org/Archives/boost/2016/11/231756.php
#if BOOST_VERSION >= 106400
#include <boost/serialization/array_wrapper.hpp>
#endif
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/covariance.hpp>
#include <boost/accumulators/statistics/density.hpp>
#include <boost/accumulators/statistics/error_of_mean.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variates/covariate.hpp>
#include <boost/make_shared.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::accumulators;
namespace bdata = boost::unit_test::data;

namespace {
    
std::vector<Period> periods = { 1*Days, 1*Years, 2*Years, 3*Years, 5*Years, 10*Years, 15*Years, 20*Years, 30*Years };
std::vector<Real> prices { 100, 101, 102, 103, 105, 110, 115, 120, 130 };

struct CommoditySchwartzModelTestData : public qle::test::TopLevelFixture {
    CommoditySchwartzModelTestData(bool driftFreeState) :
        ts(QuantLib::ext::make_shared<InterpolatedPriceCurve<Linear>>(periods, prices, ActualActual(ActualActual::ISDA), USDCurrency())) {

        referenceDate = Date(10, November, 2022);
        Settings::instance().evaluationDate() = referenceDate;

        Handle<Quote> fx(QuantLib::ext::make_shared<SimpleQuote>(1.0));
        sigma = 0.1;
        kappa = 0.05;

        std::vector<Real> times_a;
        std::vector<Real> a;

        times_a.push_back(0.1);
        times_a.push_back(1.0);
        times_a.push_back(5.0);
        times_a.push_back(10.0);

        a.push_back(0.1);
        a.push_back(0.2);
        a.push_back(0.3);
        a.push_back(0.4);
        a.push_back(0.5);

        Array times_a_a, a_a;
        times_a_a = Array(times_a.begin(), times_a.end());
        a_a = Array(a.begin(), a.end());
        
        parametrization = QuantLib::ext::make_shared<CommoditySchwartzParametrization>(USDCurrency(), "WTI", ts, fx, sigma, kappa, driftFreeState);
        QL_REQUIRE(parametrization != NULL, "CommoditySchwartzParametrization has null pointer");

        parametrizationSeason = QuantLib::ext::make_shared<CommoditySchwartzParametrization>(USDCurrency(), "WTI", ts, fx, sigma, kappa, driftFreeState, times_a_a, a_a, QuantLib::ext::make_shared<QuantLib::NoConstraint>());
        QL_REQUIRE(parametrization != NULL, "CommoditySchwartzSeasonalityParametrization has null pointer");

        // TODO test Euler discretization
        model =
            QuantLib::ext::make_shared<CommoditySchwartzModel>(parametrization, CommoditySchwartzModel::Discretization::Exact);
        modelSeason =
            QuantLib::ext::make_shared<CommoditySchwartzModel>(parametrizationSeason, CommoditySchwartzModel::Discretization::Exact);
    }

    SavedSettings backup;
    
    Date referenceDate;
    Handle<QuantExt::PriceTermStructure> ts;
    Real kappa, sigma;
    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> parametrization;
    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> parametrizationSeason;
    QuantLib::ext::shared_ptr<CommoditySchwartzModel> model;
    QuantLib::ext::shared_ptr<CommoditySchwartzModel> modelSeason;
}; // CommoditySchwarzModelTestData

} // namespace

//BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, CommoditySchwartzModelTestData)
BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(CommoditySchwartzModelTest)

std::vector<bool> driftFreeState{ true, false };
std::vector<Size> steps{ 1, 52 };

BOOST_DATA_TEST_CASE(testMartingaleProperty,
                     bdata::make(driftFreeState) * bdata::make(steps),
                     driftFreeState, steps) {

    //BOOST_AUTO_TEST_CASE(testMartingaleProperty) {
    
    BOOST_TEST_MESSAGE("Testing martingale property in the COM Schwartz model ...");

    CommoditySchwartzModelTestData data(driftFreeState);
    QuantLib::ext::shared_ptr<StochasticProcess> process = data.model->stateProcess();
    QuantLib::ext::shared_ptr<StochasticProcess> processSeason = data.modelSeason->stateProcess();
    QL_REQUIRE(process != NULL, "process has null pointer!");

    BOOST_TEST_MESSAGE("Seasonality values");
    BOOST_TEST_MESSAGE(data.modelSeason->parametrization()->m(1.0));
    BOOST_TEST_MESSAGE(data.modelSeason->parametrization()->m(2.0));
    BOOST_TEST_MESSAGE(data.modelSeason->parametrization()->m(5.0));
    Size n = 100000; // number of paths
    Size seed = 42; // rng seed
    Time t = 10.0;  // simulation horizon
    Time T = 20.0;  // forward price maturity
    //Size steps = 1; 

    TimeGrid grid(t, steps);
    LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator(steps, seed);
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);
    MultiPathGenerator<LowDiscrepancy::rsg_type> pgSeason(processSeason, grid, sg, false);
    //MultiPathGeneratorMersenneTwister pg(process, grid, seed, true);

    accumulator_set<double, stats<tag::mean, tag::variance, tag::error_of<tag::mean>>> acc_price, acc_state, acc_price_season, acc_state_season;

    Array state(1);
    Array stateSeason(1);
    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path = pg.next();
        Sample<MultiPath> pathSeason = pgSeason.next();
        Size l = path.value[0].length() - 1;
        state[0] = path.value[0][l];
        stateSeason[0] = pathSeason.value[0][l];
        Real price = data.model->forwardPrice(t, T, state);
        Real priceSeason = data.modelSeason->forwardPrice(t, T, stateSeason);
        acc_price(price);
        acc_state(state[0]);
        acc_price_season(priceSeason);
        acc_state_season(stateSeason[0]);
    }

    BOOST_TEST_MESSAGE("sigma = " << data.model->parametrization()->sigmaParameter());
    BOOST_TEST_MESSAGE("kappa = " << data.model->parametrization()->kappaParameter());
    BOOST_TEST_MESSAGE("samples = " << n);
    BOOST_TEST_MESSAGE("steps = " << steps);
    BOOST_TEST_MESSAGE("t = " << t);
    BOOST_TEST_MESSAGE("T = " << T);
    
    // Martingale test for F(t,T)
    {
        Real found = mean(acc_price);
        Real expected = data.parametrization->priceCurve()->price(T);
        Real error = error_of<tag::mean>(acc_price);
        
        BOOST_TEST_MESSAGE("Check that E[F(t,T)] = F(0,T)");
        BOOST_TEST_MESSAGE("Avg = " << found
                           << " +- " << error
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected) < error, "Martingale test failed for F(t,T) evolution, found " << found << ", expected " << expected);
    }

    // Martingale test with seasonality for F(t,T)
    {
        Real found = mean(acc_price_season);
        Real expected = data.parametrizationSeason->priceCurve()->price(T);
        Real error = error_of<tag::mean>(acc_price_season);
        
        BOOST_TEST_MESSAGE("Check that E[F(t,T)] = F(0,T)");
        BOOST_TEST_MESSAGE("Avg = " << found
                           << " +- " << error
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected) < error, "Martingale test failed for F(t,T) evolution, found " << found << ", expected " << expected);
    }
    
    // Martingale test for the state variable
    { 
        Real found = mean(acc_state);
        Real expected = 0;
        Real error = error_of<tag::mean>(acc_state);

        BOOST_TEST_MESSAGE("Check that the mean of the state variable is zero");
        BOOST_TEST_MESSAGE("Avg = " << found
                           << " +- " << error_of<tag::mean>(acc_state)
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected) < error, "Martingale test failed for the state variable, found " << found << ", expected " << expected);
    }

    // Martingale test with seasonalty for the state variable
    { 
        Real found = mean(acc_state_season);
        Real expected = 0;
        Real error = error_of<tag::mean>(acc_state_season);

        BOOST_TEST_MESSAGE("Check that the mean of the state variable is zero");
        BOOST_TEST_MESSAGE("Avg = " << found
                           << " +- " << error_of<tag::mean>(acc_state)
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected) < error, "Martingale test failed for the state variable, found " << found << ", expected " << expected);
    }

    // Variance test for the state variable, implicit in the first test above 
    {
        Real found = variance(acc_state);
        Real expected = data.parametrization->variance(t);
        // FIXME: What's the MC error here? I assume that the mean error is smaller than the error of variance estimate, so re-using it.
        Real error = error_of<tag::mean>(acc_state); 

        QuantLib::ext::shared_ptr<CommoditySchwartzStateProcess> stateProcess = QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzStateProcess>(process);
        QL_REQUIRE(stateProcess, "state process is null");
        Real expected2 = stateProcess->variance(0.0, 0.0, t);

        // consistency check, should be identical
        BOOST_TEST(fabs(expected - expected2) < 1e-10, "Inconsistent state variable variance " << expected << " vs " << expected2);
                    
        BOOST_TEST_MESSAGE("Check that the variance of the state variable matches expectation");
        BOOST_TEST_MESSAGE("Var = " << found
                           << " +- " << error
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected2) < error,
                   "Simulated variance of the state variable does match expectation, found " << found << ", expected " << expected);
        
    }

    // Variance test for the state variable with seasonality, implicit in the first test above 
    {
        Real found = variance(acc_state_season);
        Real expected = data.parametrizationSeason->variance(t);
        // FIXME: What's the MC error here? I assume that the mean error is smaller than the error of variance estimate, so re-using it.
        Real error = error_of<tag::mean>(acc_state_season); 

        QuantLib::ext::shared_ptr<CommoditySchwartzStateProcess> stateProcessSeason = QuantLib::ext::dynamic_pointer_cast<CommoditySchwartzStateProcess>(processSeason);
        QL_REQUIRE(stateProcessSeason, "state process is null");
        Real expected2 = stateProcessSeason->variance(0.0, 0.0, t);

        // consistency check, should be identical
        BOOST_TEST(fabs(expected - expected2) < 1e-10, "Inconsistent state variable variance " << expected << " vs " << expected2);
                    
        BOOST_TEST_MESSAGE("Check that the variance of the state variable matches expectation");
        BOOST_TEST_MESSAGE("Var = " << found
                           << " +- " << error
                           << " vs expected " << expected);
        BOOST_TEST(fabs(found - expected2) < error,
                   "Simulated variance of the state variable does match expectation, found " << found << ", expected " << expected);
        
    }
    
} // testMartingaleProperty

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
