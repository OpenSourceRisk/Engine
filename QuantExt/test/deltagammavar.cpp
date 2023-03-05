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
#include <boost/test/unit_test.hpp>

#include <qle/math/deltagammavar.hpp>

#include <qle/math/deltagammavar.hpp>

#include <boost/make_shared.hpp>
#include <boost/math/distributions/chi_squared.hpp>

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

    std::vector<Real> quantiles = {0.9, 0.95, 0.99, 0.999, 0.9999};

    BOOST_TEST_MESSAGE("Run MC simulation...");
    Matrix nullGamma(dim, dim, 0.0);
    auto mc1All = deltaGammaVarMc<PseudoRandom>(omega, delta, nullGamma, quantiles, paths, seedMc);
    auto mc2All = deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, quantiles, paths, seedMc);
    BOOST_TEST_MESSAGE("MC simulation Done.");

    BOOST_TEST_MESSAGE("      Quantile      dVaR(MC)      dgVaR(MC)    dVaR(Mdl)     dgVaR(CF)     dgVaR(SD)      "
                       "err(CF)%      err(SD)%");
    BOOST_TEST_MESSAGE("==============================================================================================="
                       "=================");

    Size i = 0;
    for (auto const& q : quantiles) {

        Real mc1 = mc1All[i];
        Real mc2 = mc2All[i++];

        Real dVar = deltaVar(omega, delta, q);
        Real dgVarCf = deltaGammaVarCornishFisher(omega, delta, gamma, q);
        Real dgVarSd = deltaGammaVarSaddlepoint(omega, delta, gamma, q);
        Real errCf = (dgVarCf - mc2) / mc2 * 100.0;
        Real errSd = (dgVarSd - mc2) / mc2 * 100.0;

        BOOST_TEST_MESSAGE(std::right << std::setw(14) << q << std::setw(14) << mc1 << std::setw(14) << mc2
                                      << std::setw(14) << dVar << std::setw(14) << dgVarCf << std::setw(14) << dgVarSd
                                      << std::setw(14) << errCf << std::setw(14) << errSd);

        BOOST_CHECK_CLOSE(dVar, mc1, 5.0);
        BOOST_CHECK_CLOSE(dgVarCf, mc2, 15.0);
        BOOST_CHECK_CLOSE(dgVarSd, mc2, 5.0);
    }

    BOOST_TEST_MESSAGE("==============================================================================================="
                       "===============\n\n");

} // test
} // anonymous namespace

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(DeltaGammaVarTest)

BOOST_AUTO_TEST_CASE(testDeltaGammaVar) {

    // TODO test Daniel's approx. explicitly (khat < 1E-5)
    // TODO add more test cases (negative gammas, higher dimensions)

    Size n = (Size)1E6;

    test(1, true, false, 42, 42, n);
    test(1, false, true, 42, 42, n);
    test(1, true, true, 42, 42, n);

    test(2, true, false, 42, 42, n);
    test(2, false, true, 42, 42, n);
    test(2, true, true, 42, 42, n);

    test(10, true, false, 42, 42, n);
    test(10, false, true, 42, 42, n);
    test(10, true, true, 42, 42, n);

    // fewer paths here
    test(100, true, false, 42, 42, n);
    test(100, false, true, 42, 42, n);
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
    Real var_cf = deltaGammaVarCornishFisher(omega, delta, gamma_m, p);
    Real var_sd = deltaGammaVarSaddlepoint(omega, delta, gamma_m, p);

    Real refVal = 0.5 * gamma * boost::math::quantile(chisq, 1.0 - p);

    BOOST_TEST_MESSAGE("mc  = " << var_mc);
    BOOST_TEST_MESSAGE("cf  = " << var_cf);
    BOOST_TEST_MESSAGE("sd  = " << var_sd);
    BOOST_TEST_MESSAGE("ref = " << refVal);

    // the AS269 function in R (pacakge PDQutils) produces this
    BOOST_CHECK_SMALL(std::abs(-4707.882 - var_cf), 0.001);

    BOOST_CHECK_SMALL(std::abs(refVal - var_sd), 0.5);
    BOOST_CHECK_SMALL(std::abs(refVal - var_mc), 0.5);
}

BOOST_AUTO_TEST_CASE(testCase001) {
    // fails as of 05-Sep-2017, fixed with commit 71c736873
    BOOST_TEST_MESSAGE("Running regression test case 001...");
    std::vector<double> d1{691.043, 8.62406, 9706.97, 0, 0};
    std::vector<double> d2 = {-13.9605, 0, 0, 0, 0, 0, -0.174223, 0, 0, 0, 0, 0, -196.1,
                              0,        0, 0, 0, 0, 0, 0,         0, 0, 0, 0, 0};
    std::vector<double> d3 = {96.3436,   -0.828459, -6.59142,  0.583848, -0.0639266, -0.828459, 97.7309,
                              12.4906,   -2.03511,  -0.504752, -6.59142, 12.4906,    95.12,     0.800706,
                              0.443861,  0.583848,  -2.03511,  0.800706, 2.71239,    0.288881,  -0.0639266,
                              -0.504752, 0.443861,  0.288881,  1.42701};
    Array delta(d1.begin(), d1.end());
    Matrix gamma(5, 5, d2.begin(), d2.end());
    Matrix omega(5, 5, d3.begin(), d3.end());
    Real var = deltaGammaVarSaddlepoint(omega, delta, gamma, 0.99);
    Real var_mc = deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, 0.99, 1000000, 42);
    BOOST_TEST_MESSAGE("sd = " << var);
    BOOST_TEST_MESSAGE("mc = " << var_mc);
    BOOST_CHECK_CLOSE(var, var_mc, 0.5);
}

BOOST_AUTO_TEST_CASE(testCase002) {
    // failed as of 05-Sep-2018
    BOOST_TEST_MESSAGE("Running regression test case 002...");

    // similar setup to testNegativeGamma(), but with higher variance and positive gamma
    boost::math::chi_squared_distribution<Real> chisq(1.0);

    Array delta(1, 0.0);
    Matrix gamma(1, 1, 1.0);
    Matrix omega(1, 1, 1.0E6);
    Real p = 0.99;

    Real var_sd = deltaGammaVarSaddlepoint(omega, delta, gamma, p);
    Real refVal = 0.5 * 1E6 * boost::math::quantile(chisq, p);
    Real var_mc = deltaGammaVarMc<PseudoRandom>(omega, delta, gamma, p, 1000000, 42);

    BOOST_TEST_MESSAGE("sd  = " << var_sd);
    BOOST_TEST_MESSAGE("mc  = " << var_mc);
    BOOST_TEST_MESSAGE("ref = " << refVal);

    BOOST_CHECK_CLOSE(refVal, var_sd, 1.0);
}

BOOST_AUTO_TEST_CASE(testCase003) {
    // failed in the Q3-BT 2018
    BOOST_TEST_MESSAGE("Running regression test case 003...");

    // Salvaged covariance matrix
    vector<Real> data{
        11.357910440165, 0.301883284121,  1.690565094559,  0.028921366715,  1.445952963717,  2.683104461304,
        2.975281862097,  0.284778609491,  3.977509941411,  2.700492214606,  3.973616990595,  1.842042909216,
        4.507630684591,  2.188574976899,  1.839870252197,  1.653581957759,  0.301883284121,  1.545170636908,
        1.140841711137,  0.011046063984,  0.223680967726,  0.926416050189,  0.637660185458,  1.634591590508,
        0.328944419108,  -0.321306872147, 0.490226761216,  -0.228872084003, -0.466954542255, 0.908486575719,
        1.208420546045,  1.057641385035,  1.690565094559,  1.140841711137,  4.590122578368,  0.016138328046,
        1.681703229533,  2.722501254257,  2.941664204785,  4.088913570167,  0.753207493946,  0.750845717386,
        2.327852200648,  -0.083383166375, 0.791187240585,  3.432410532424,  4.576488134589,  4.484608491882,
        0.028921366715,  0.011046063984,  0.016138328046,  0.003321196784,  -0.002052702116, 0.015027345216,
        0.025225956444,  0.009488292952,  0.023036962659,  0.003752073385,  0.025786900243,  0.004902517590,
        0.017973354395,  0.013380695889,  0.016450708955,  0.015793906816,  1.445952963717,  0.223680967726,
        1.681703229533,  -0.002052702116, 5.051235107717,  1.504449552996,  1.411232675828,  -0.066709336186,
        0.740371793190,  0.922017945905,  1.423545223509,  0.592880234568,  1.318219401490,  1.512119827471,
        1.709049165903,  1.651695766740,  2.683104461304,  0.926416050189,  2.722501254257,  0.015027345216,
        1.504449552996,  5.896840384423,  2.736094553275,  2.318419864064,  2.683772279561,  0.869866043995,
        2.795123680474,  0.233284568353,  1.954971283941,  2.638478800353,  2.962854214006,  2.999498221322,
        2.975281862097,  0.637660185458,  2.941664204785,  0.025225956444,  1.411232675828,  2.736094553275,
        3.730376993077,  2.574152890379,  2.332889913708,  1.012628826625,  2.401055859072,  0.425908702537,
        2.104222769101,  2.846640151863,  3.254007639538,  3.174532782066,  0.284778609491,  1.634591590508,
        4.088913570167,  0.009488292952,  -0.066709336186, 2.318419864064,  2.574152890379,  8.346545067056,
        0.111146561779,  0.061442388170,  1.440123839301,  -0.532880568392, -0.363306461515, 3.164184153356,
        4.239774387395,  4.222873062980,  3.977509941411,  0.328944419108,  0.753207493946,  0.023036962659,
        0.740371793190,  2.683772279561,  2.332889913708,  0.111146561779,  7.816500777128,  1.306158501267,
        1.892772141649,  1.533232314993,  2.966214070512,  1.981467787886,  0.858479274405,  0.688098796909,
        2.700492214606,  -0.321306872147, 0.750845717386,  0.003752073385,  0.922017945905,  0.869866043995,
        1.012628826625,  0.061442388170,  1.306158501267,  3.255129750500,  1.447265157820,  1.646805443131,
        2.024131319493,  0.823110422895,  0.661139572160,  0.726470332699,  3.973616990595,  0.490226761216,
        2.327852200648,  0.025786900243,  1.423545223509,  2.795123680474,  2.401055859072,  1.440123839301,
        1.892772141649,  1.447265157820,  9.307941714908,  1.003272291798,  2.960270274699,  2.637484069448,
        2.395034154720,  2.407859255045,  1.842042909216,  -0.228872084003, -0.083383166375, 0.004902517590,
        0.592880234568,  0.233284568353,  0.425908702537,  -0.532880568392, 1.533232314993,  1.646805443131,
        1.003272291798,  2.715163222630,  1.397890009325,  0.370386734188,  -0.170205201951, -0.252456813317,
        4.507630684591,  -0.466954542255, 0.791187240585,  0.017973354395,  1.318219401490,  1.954971283941,
        2.104222769101,  -0.363306461515, 2.966214070512,  2.024131319493,  2.960270274699,  1.397890009325,
        12.145104529503, 1.291504589690,  0.751431588477,  0.731038259921,  2.188574976899,  0.908486575719,
        3.432410532424,  0.013380695889,  1.512119827471,  2.638478800353,  2.846640151863,  3.164184153356,
        1.981467787886,  0.823110422895,  2.637484069448,  0.370386734188,  1.291504589690,  3.304040607934,
        3.654236283615,  3.549318308813,  1.839870252197,  1.208420546045,  4.576488134589,  0.016450708955,
        1.709049165903,  2.962854214006,  3.254007639538,  4.239774387395,  0.858479274405,  0.661139572160,
        2.395034154720,  -0.170205201951, 0.751431588477,  3.654236283615,  5.109964090288,  4.959183419230,
        1.653581957759,  1.057641385035,  4.484608491882,  0.015793906816,  1.651695766740,  2.999498221322,
        3.174532782066,  4.222873062980,  0.688098796909,  0.726470332699,  2.407859255045,  -0.252456813317,
        0.731038259921,  3.549318308813,  4.959183419230,  5.024852085655};
    Matrix covar(16, 16, data.begin(), data.end());

    // Gamma matrix, all 0 except g[15,15] = -0.000000000262
    Matrix gamma(16, 16, 0.0);
    gamma[14][14] = -0.000000000262;

    // Delta array, all 0 except d[15] = -247189.692289613
    Array delta(16, 0.0);
    delta[14] = -247189.692289613;

    // Try the saddlepoint VAR
    Real p = 0.99;
    Real sdvar = deltaGammaVarSaddlepoint(covar, delta, gamma, p);
    BOOST_TEST_MESSAGE("sdvar=" << sdvar);

    // Try the monte-carlo VAR
    Real mcvar = deltaGammaVarMc<PseudoRandom>(covar, delta, gamma, p, 1000000, 42);
    BOOST_TEST_MESSAGE("mcvar=" << mcvar);

    // Check saddlepoint and monte-carlo results are close
    BOOST_CHECK_CLOSE(sdvar, mcvar, 1.0);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
