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
#include <boost/foreach.hpp>
#include <boost/make_shared.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/test/unit_test.hpp>
#include <qle/math/deltagammavar.hpp>

using namespace QuantLib;
using namespace QuantExt;

using namespace boost::unit_test_framework;
using std::vector;

namespace {
void test(const Size dim, const bool nonzeroDelta, const bool nonzeroGamma, const Size seedParam, const Size seedMc,
          const Size paths) {

    BOOST_TEST_MESSAGE("################ Testing delta gamma VaR, dim=" << dim << ", delta=" << nonzeroDelta
                                                                        << ", gamma=" << nonzeroGamma
                                                                        << ", paths=" << paths << "\n");

    MersenneTwisterUniformRng mt(seedParam);

    // generate random covariance matrix

    BOOST_TEST_MESSAGE("Generate transformation matrix L");
    Matrix L(dim, dim, 0.0);
    Real det = 0.0;
    do {
        for (Size i = 0; i < dim; ++i) {
            for (Size j = 0; j < dim; ++j) {
                L[i][j] = mt.nextReal();
            }
        }
        det = determinant(L);
        BOOST_TEST_MESSAGE("... done, determinant is " << det);
    } while (close_enough(det, 0.0));

    Matrix omega = transpose(L) * L;

    // scale entries such that they have order of magnitude 0.1
    Real num = QuantExt::detail::absMax(omega);
    omega /= num * 10.0;

    // generate random delta vector

    BOOST_TEST_MESSAGE("Generate delta");
    Array delta(dim, 0.0);
    if (nonzeroDelta) {
        for (Size i = 0; i < dim; ++i) {
            delta[i] = mt.nextReal() * 1000.0 - 500.0;
        }
    }

    // generate random gamma matrix, TODO negative gammas?

    BOOST_TEST_MESSAGE("Generate gamma");
    Matrix gamma(dim, dim, 0.0), shift(dim, dim, 0.0);
    if (nonzeroGamma) {
        for (Size i = 0; i < dim; ++i) {
            for (Size j = 0; j < i; ++j) {
                gamma[i][j] = gamma[j][i] = mt.nextReal() * 1000.0; // - 500.0;
            }
            gamma[i][i] = mt.nextReal() * 1000.0; // - 500.0;
        }
    }

    BOOST_TEST_MESSAGE("delta=" << delta);
    if (gamma.rows() <= 20) {
        BOOST_TEST_MESSAGE("\ngamma=\n" << gamma);
        BOOST_TEST_MESSAGE("omega=\n" << omega);
    } else {
        BOOST_TEST_MESSAGE("\ngamma= too big to display (" << gamma.rows() << "x" << gamma.columns() << ")");
        BOOST_TEST_MESSAGE("omega= too big to display (" << omega.rows() << "x" << omega.columns() << ")\n");
    }

    // check results against MC simulation

    std::vector<Real> quantiles;
    quantiles.push_back(0.9);
    quantiles.push_back(0.95);
    quantiles.push_back(0.99);
    quantiles.push_back(0.999);
    quantiles.push_back(0.9999);

    BOOST_TEST_MESSAGE("Run MC simulation...");
    Matrix nullGamma(dim, dim, 0.0);
    std::vector<Real> mc1All = deltaGammaVarMc<PseudoRandom>(omega, delta, nullGamma, quantiles, paths, seedMc);
    std::vector<Real> mc2All = deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, quantiles, paths, seedMc);
    BOOST_TEST_MESSAGE("MC simulation Done.");

    BOOST_TEST_MESSAGE("      Quantile      dVaR(MC)      dgVaR(MC)    dVaR(Mdl)");
    BOOST_TEST_MESSAGE("========================================================");

    Size i = 0;
    BOOST_FOREACH (Real q, quantiles) {

        Real mc1 = mc1All[i];
        Real mc2 = mc2All[i++];

        Real dVar = deltaVar(omega, delta, q);

        BOOST_TEST_MESSAGE(std::right << std::setw(14) << q << std::setw(14) << mc1 << std::setw(14) << mc2
                                      << std::setw(14) << dVar);

        BOOST_CHECK_CLOSE(dVar, mc1, 5.0);
    }

    BOOST_TEST_MESSAGE("========================================================\n\n");

} // test
} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DeltaGammaVarTest)

BOOST_AUTO_TEST_CASE(testDeltaGammaVar) {

    // TODO add more test cases (negative gammas, higher dimensions)

    // for better accuracy use 1E7 (1E6) paths below

    Size n = (Size)1E5;

    test(1, true, false, 42, 42, n);
    test(1, true, true, 42, 42, n);

    test(2, true, false, 42, 42, n);
    test(2, true, true, 42, 42, n);

    test(10, true, false, 42, 42, n);
    test(10, true, true, 42, 42, n);

    test(100, true, false, 42, 42, n);
    test(100, true, true, 42, 42, n);
}

BOOST_AUTO_TEST_CASE(testNegativeGamma) {

    BOOST_TEST_MESSAGE("Testing delta gamma var for pl = -u^2, u standard normal...");

    // choose n=1, gamma=-10k, omega = 1, then the pl is -0.5*u^2 with
    // u standard normal, in other words  -2pl is chi-squared
    // distributed with one degree of freedom

    boost::math::chi_squared_distribution<Real> chisq(1.0);

    Real gamma = -10000.0;

    Array delta(1, 0.0);
    Matrix gamma_m(1, 1, gamma);
    Matrix omega(1, 1, 1.0);

    Real p = 0.99;

    Real var_mc = deltaGammaVarMc<PseudoRandom>(omega, delta, gamma_m, p, 1000000, 142);

    Real refVal = 0.5 * gamma * boost::math::quantile(chisq, 1.0 - p);

    BOOST_TEST_MESSAGE("mc  = " << var_mc);
    BOOST_TEST_MESSAGE("ref = " << refVal);

    BOOST_CHECK_SMALL(std::abs(refVal - var_mc), 0.5);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
