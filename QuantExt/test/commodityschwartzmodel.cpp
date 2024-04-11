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

        parametrization = QuantLib::ext::make_shared<CommoditySchwartzParametrization>(USDCurrency(), "WTI", ts, fx, sigma, kappa, driftFreeState);
        QL_REQUIRE(parametrization != NULL, "CommoditySchwartzParametrization has null pointer");

        // TODO test Euler discretization
        model =
            QuantLib::ext::make_shared<CommoditySchwartzModel>(parametrization, CommoditySchwartzModel::Discretization::Exact);
    }

    SavedSettings backup;
    
    Date referenceDate;
    Handle<QuantExt::PriceTermStructure> ts;
    Real kappa, sigma;
    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> parametrization;
    QuantLib::ext::shared_ptr<CommoditySchwartzModel> model;
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
    QL_REQUIRE(process != NULL, "process has null pointer!");

    Size n = 100000; // number of paths
    Size seed = 42; // rng seed
    Time t = 10.0;  // simulation horizon
    Time T = 20.0;  // forward price maturity
    //Size steps = 1; 

    TimeGrid grid(t, steps);
    LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator(steps, seed);
    MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);
    //MultiPathGeneratorMersenneTwister pg(process, grid, seed, true);

    accumulator_set<double, stats<tag::mean, tag::variance, tag::error_of<tag::mean>>> acc_price, acc_state;

    Array state(1);
    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path = pg.next();
        Size l = path.value[0].length() - 1;
        state[0] = path.value[0][l];
        Real price = data.model->forwardPrice(t, T, state);
        acc_price(price);
        acc_state(state[0]);
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
    
} // testMartingaleProperty


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
