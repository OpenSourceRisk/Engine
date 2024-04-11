/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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
#include <ql/currencies/europe.hpp>
#include <ql/experimental/callablebonds/callablebond.hpp>
#include <ql/indexes/ibor/euribor.hpp>
#include <ql/instruments/callabilityschedule.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/math/optimization/levenbergmarquardt.hpp>
#include <ql/math/randomnumbers/rngtraits.hpp>
#include <ql/math/statistics/incrementalstatistics.hpp>
#include <ql/methods/montecarlo/multipathgenerator.hpp>
#include <ql/methods/montecarlo/pathgenerator.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yield/flatforward.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actualactual.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/pricingengines/credit/midpointcdsengine.hpp>

#include <qle/instruments/cdsoption.hpp>
#include <qle/methods/multipathgeneratorbase.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/models/cdsoptionhelper.hpp>
#include <qle/models/crcirpp.hpp>
#include <qle/models/cirppconstantfellerparametrization.hpp>
#include <qle/models/crossassetmodel.hpp>
#include <qle/processes/crcirppstateprocess.hpp>

#include <boost/make_shared.hpp>

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

namespace {
struct CreditModelTestData_flat: public qle::test::TopLevelFixture {
    CreditModelTestData_flat()
        : referenceDate(29, July, 2017), dts(QuantLib::ext::make_shared<FlatHazardRate>(referenceDate, 0.04, ActualActual(ActualActual::ISDA))),
          yts(QuantLib::ext::make_shared<FlatForward>(referenceDate, 0.02, ActualActual(ActualActual::ISDA))) {

        Settings::instance().evaluationDate() = referenceDate;

        kappa = 0.206;
        theta = 0.04;
        sigma = sqrt(2 * kappa * theta) - 1E-10;
        y0 = theta;
        shifted = true;

        recoveryRate = 0.4;
        // QL_REQUIRE(2 * eurKappa * eurTheta / eurSigma / eurSigma > 1 , "Feller Condition is not satisfied!");
        cirParametrization = QuantLib::ext::make_shared<CrCirppConstantWithFellerParametrization>(
            EURCurrency(), dts, kappa, theta, sigma, y0, shifted);
        QL_REQUIRE(cirParametrization != NULL, "CrCirppConstantWithFellerParametrization has null pointer!");

        model = QuantLib::ext::make_shared<CrCirpp>(cirParametrization);
        BOOST_TEST_MESSAGE("CIR++ parameters: ");
        BOOST_TEST_MESSAGE("Kappa: \t" << model->parametrization()->kappa(0));
        BOOST_TEST_MESSAGE("Theta: \t" << model->parametrization()->theta(0));
        BOOST_TEST_MESSAGE("Sigma: \t" << model->parametrization()->sigma(0));
        BOOST_TEST_MESSAGE("y0: \t" << model->parametrization()->y0(0));
        BOOST_TEST_MESSAGE("Feller condition is (>1 ok) "
                           << 2.0 * model->parametrization()->kappa(0) * model->parametrization()->theta(0) /
                                  model->parametrization()->sigma(0) / model->parametrization()->sigma(0));
    }

    SavedSettings backup;
    Date referenceDate;
    Handle<DefaultProbabilityTermStructure> dts;
    Handle<YieldTermStructure> yts;
    Real kappa, theta, sigma, y0;
    bool shifted;
    Real recoveryRate;
    QuantLib::ext::shared_ptr<CrCirppConstantWithFellerParametrization> cirParametrization;
    QuantLib::ext::shared_ptr<CrCirpp> model;
}; // IrTSModelTestData
} // namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, CreditModelTestData_flat)

BOOST_AUTO_TEST_SUITE(CrCirppModelTest)

BOOST_AUTO_TEST_CASE(testMartingaleProperty) {

    BOOST_TEST_MESSAGE("Testing martingale property in credit-CIR++ model for Brigo-Alfonsi discretizations...");

    QuantLib::ext::shared_ptr<StochasticProcess> process = model->stateProcess();
    QL_REQUIRE(process != NULL, "process has null pointer!");

    // CIRplusplusStateProcess::Discretization discType = CIRplusplusStateProcess::Discretization::Reflection;
    // CIRplusplusStateProcess::Discretization discType = CIRplusplusStateProcess::Discretization::PartialTruncation;
    // CIRplusplusStateProcess::Discretization discType = CIRplusplusStateProcess::Discretization::FullTruncation;
    // QuantLib::ext::shared_ptr<StochasticProcess> process = QuantLib::ext::make_shared<CIRplusplusStateProcess>(model.get(),
    // discType);  BOOST_TEST_MESSAGE("Simulation type of negative variance process "<<discType);
    Size n = 10000; // number of paths
    Size seed = 42; // rng seed
    // Time T = 25.0;                          // maturity of payoff
    // Time T2 = 40.0;                         // zerobond maturity
    Time T = 10.0;  // maturity of payoff
    Time T2 = 20.0; // zerobond maturity
    // QL_REQUIRE(T2 == model->maxSimulationHorizon(), "Forward measure horizon must be equalt to the maturity of the
    // zero bond");
    Size steps = static_cast<Size>(T * 52); // number of steps taken (euler)

    TimeGrid grid(T, steps);
    // LowDiscrepancy::rsg_type sg = LowDiscrepancy::make_sequence_generator( 9 * steps, seed);
    // MultiPathGenerator<LowDiscrepancy::rsg_type> pg(process, grid, sg, false);
    MultiPathGeneratorMersenneTwister pg(process, grid, seed, true);
    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean>>> meanTest_y;
    accumulator_set<double, stats<tag::variance, tag::error_of<tag::mean>>> varTest_y;

    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean>>> sp;
    accumulator_set<double, stats<tag::mean, tag::error_of<tag::mean>>> numeraire;

    accumulator_set<double, stats<tag::density>> histXAcc(tag::density::cache_size = 10000,
                                                          tag::density::num_bins = 50);

    // typedef boost::iterator_range<std::vector<std::pair<double, double>>::iterator> histogram_type;

    // //create an accumulator
    // acc histXAcc( tag::density::num_bins = 20, tag::density::cache_size = 10);

    std::ofstream of;
    // of.open("y_paths.txt");
    for (Size j = 0; j < n; ++j) {
        Sample<MultiPath> path = pg.next();
        Size l = path.value[0].length() - 1;
        Real y = path.value[0][l];
        Real num = path.value[1][l];
        sp(model->survivalProbability(T, T2, y) * num);
        numeraire(num);
        meanTest_y(y);
        varTest_y(y);
        histXAcc(y);
    }
    // histogram_type histX = density(histXAcc);
    // double total = 0.0;
    // std::ofstream ofHistX;
    // ofHistX.open("y_hist.txt");

    // Real binSize = histX[1].first - histX[0].first;
    // for (Size i = 0; i < histX.size(); i++) {
    //     ofHistX << histX[i].first << "," << histX[i].second / binSize << std::endl;
    //     total += histX[i].second;
    // }
    // BOOST_TEST_MESSAGE("Total cdf(x): " << total); // should be 1 (and it is)

    // total = 0.0;
    // std::ofstream ofPDFX;
    // ofPDFX.open("y_pdf.txt");
    // // for (Real x = - 5.0; x <= 7.0; x += 0.1)
    // for (Real x = 0.00018; x <= 0.01; x += 0.001) {
    //     ofPDFX << x << "," << model->density(x, T) << std::endl;
    //     total += model->density(x, T);
    // }
    // BOOST_TEST_MESSAGE("Total cdf(x): " << total); // should be 1 (and it is)

    // of.close();
    // ofPDFX.close();
    // ofHistX.close();

    // Real kappa = model->parametrization()->kappa(0);
    // Real theta = model->parametrization()->theta(0);
    // Real sigma = model->parametrization()->sigma(0);
    // Real y0 = model->parametrization()->y0(0);
    // Real sigma2 = sigma * sigma;

    BOOST_TEST_MESSAGE("\nBrigo-Alfonsi:");
    // BOOST_TEST_MESSAGE("Mean = " << mean(meanTest_y) << " +- " << error_of<tag::mean>(meanTest_y) << " analytic "
    // <<y0 * exp(-kappa*T) + theta * (1- exp(-kappa*T))); BOOST_TEST_MESSAGE("Variance = " << variance(varTest_y) << "
    // +- " << error_of<tag::mean>(varTest_y)<<" analytic " <<y0*sigma2/theta*(exp(-kappa*T) - exp(-2*kappa*T)) +
    // theta*sigma2/2/kappa*(1- exp(-2*kappa *T))*(1- exp(-2*kappa *T)) );
    BOOST_TEST_MESSAGE("SP = " << mean(sp) << " +- " << error_of<tag::mean>(sp) << " vs analytical "
                               << dts->survivalProbability(T2));
    BOOST_TEST_MESSAGE("Num = " << mean(numeraire) << " +- " << error_of<tag::mean>(numeraire) << " vs analytical "
                                << dts->survivalProbability(T));

    Real tol2 = 12.0E-4;
    Real expectedSP = dts->survivalProbability(T);
    Real expectedCondSP = dts->survivalProbability(T2);
    if (std::abs(mean(numeraire) - expectedSP) > tol2)
        BOOST_FAIL("Martingale test failed for SP(t) (Brigo-Alfonsi discr.), expected "
                   << expectedSP << ", got " << mean(numeraire) << ", tolerance " << tol2);
    if (std::abs(mean(sp) - expectedCondSP) > tol2)
        BOOST_FAIL("Martingale test failed for  SP(t,T) (Brigo-Alfonsi discr.), expected "
                   << expectedCondSP << ", got " << mean(sp) << ", tolerance " << tol2);

} // testIrTSMartingaleProperty


BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
