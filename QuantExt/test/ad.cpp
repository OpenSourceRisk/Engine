/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/ad/backwardderivatives.hpp>
#include <qle/ad/forwardderivatives.hpp>
#include <qle/ad/forwardevaluation.hpp>
#include <qle/ad/ssaform.hpp>
#include <qle/math/randomvariable_ops.hpp>

#include <ql/math/distributions/normaldistribution.hpp>
#include <ql/math/randomnumbers/inversecumulativerng.hpp>
#include <ql/math/randomnumbers/mt19937uniformrng.hpp>

#include <boost/test/unit_test.hpp>

using namespace QuantExt;

BOOST_FIXTURE_TEST_SUITE(QuantExtTestSuite, qle::test::TopLevelFixture)

BOOST_AUTO_TEST_SUITE(AdTest)

BOOST_AUTO_TEST_CASE(testForwardEvaluation) {

    constexpr Real tol = 1E-14;

    // z = x+y, z = ux = (x+y)x = x^2+yx
    ComputationGraph g;
    auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
    auto y = cg_var(g, "y", ComputationGraph::VarDoesntExist::Create);
    auto u = cg_add(g, x, y, "u");
    auto z = cg_mult(g, u, x, "z");

    BOOST_TEST_MESSAGE("SSA Form:\n" + ssaForm(g, getRandomVariableOpLabels()));

    std::vector<RandomVariable> values(g.size(), RandomVariable(1, 0.0));
    values[x] = RandomVariable(1, 2.0);
    values[y] = RandomVariable(1, 3.0);

    forwardEvaluation(g, values, getRandomVariableOps(1), RandomVariable::deleter, false);

    // BOOST_CHECK_CLOSE(values[x][0], 2.0, tol); // deleted
    // BOOST_CHECK_CLOSE(values[y][0], 3.0, tol);
    // BOOST_CHECK_CLOSE(values[u][0], 5.0, tol);
    BOOST_CHECK_CLOSE(values[z][0], 10.0, tol);
}

BOOST_AUTO_TEST_CASE(testBackwardDerivatives) {

    constexpr Real tol = 1E-14;

    // z = x+y, z = ux = (x+y)x = x^2+yx
    ComputationGraph g;
    auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
    auto y = cg_var(g, "y", ComputationGraph::VarDoesntExist::Create);
    auto u = cg_add(g, x, y, "u");
    auto z = cg_mult(g, u, x, "z");

    BOOST_TEST_MESSAGE("SSA Form:\n" + ssaForm(g, getRandomVariableOpLabels()));

    // forward evaluation

    std::vector<RandomVariable> values(g.size(), RandomVariable(1, 0.0));
    values[x] = RandomVariable(1, 2.0);
    values[y] = RandomVariable(1, 3.0);

    forwardEvaluation(g, values, getRandomVariableOps(1), RandomVariable::deleter, true,
                      getRandomVariableOpNodeRequirements());

    // BOOST_CHECK_CLOSE(values[x][0], 2.0, tol); // deleted
    // BOOST_CHECK_CLOSE(values[y][0], 3.0, tol);
    // BOOST_CHECK_CLOSE(values[u][0], 5.0, tol);
    BOOST_CHECK_CLOSE(values[z][0], 10.0, tol);

    // backward derivatives

    std::vector<RandomVariable> derivativesBwd(g.size(), RandomVariable(1, 0.0));
    derivativesBwd[z] = RandomVariable(1, 1.0);

    std::vector<bool> keep(g.size(), false);
    keep[x] = true;
    keep[y] = true;
    keep[z] = true;
    keep[u] = true;

    backwardDerivatives(g, values, derivativesBwd, getRandomVariableGradients(1), RandomVariable::deleter, keep);

    // dz/dx = 2x+y
    BOOST_CHECK_CLOSE(derivativesBwd[x][0], 7.0, tol);
    // dz/dy = x
    BOOST_CHECK_CLOSE(derivativesBwd[y][0], 2.0, tol);
    // dz/du = x
    BOOST_CHECK_CLOSE(derivativesBwd[u][0], 2.0, tol);
    // dz/dz = 1
    BOOST_CHECK_CLOSE(derivativesBwd[z][0], 1.0, tol);
}

BOOST_AUTO_TEST_CASE(testForwardDerivatives) {

    constexpr Real tol = 1E-14;

    // z = x+y, z = ux = (x+y)x = x^2+yx
    ComputationGraph g;
    auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
    auto y = cg_var(g, "y", ComputationGraph::VarDoesntExist::Create);
    auto u = cg_add(g, x, y, "u");
    auto z = cg_mult(g, u, x, "z");

    BOOST_TEST_MESSAGE("SSA Form:\n" + ssaForm(g, getRandomVariableOpLabels()));

    // forward evaluation

    std::vector<RandomVariable> values(g.size(), RandomVariable(1, 0.0));
    values[x] = RandomVariable(1, 2.0);
    values[y] = RandomVariable(1, 3.0);

    forwardEvaluation(g, values, getRandomVariableOps(1), RandomVariable::deleter, true,
                      getRandomVariableOpNodeRequirements());

    // forward derivatives

    std::vector<RandomVariable> derivativesFwdX(g.size(), RandomVariable(1, 0.0));
    std::vector<RandomVariable> derivativesFwdY(g.size(), RandomVariable(1, 0.0));
    derivativesFwdX[x] = RandomVariable(1, 1.0);
    derivativesFwdY[y] = RandomVariable(1, 1.0);

    forwardDerivatives(g, values, derivativesFwdX, getRandomVariableGradients(1), RandomVariable::deleter);
    forwardDerivatives(g, values, derivativesFwdY, getRandomVariableGradients(1), RandomVariable::deleter);

    // dz/dx = 2x+y
    BOOST_CHECK_CLOSE(derivativesFwdX[z][0], 7.0, tol);
    // dz/dy = x
    BOOST_CHECK_CLOSE(derivativesFwdY[z][0], 2.0, tol);
}

BOOST_AUTO_TEST_CASE(testIndicatorDerivative) {
    BOOST_TEST_MESSAGE("Testing indicator derivative...");

    Size n = 5000000;    // number of samples
    Real epsilon = 0.05; // indcator derivative bandwidth

    ComputationGraph g;
    auto z = cg_var(g, "z", ComputationGraph::VarDoesntExist::Create); // z ~ N(z0,1)
    auto y = cg_indicatorGt(g, z, cg_const(g, 0.0));                   // ind = 1_{z>0}

    InverseCumulativeRng<MersenneTwisterUniformRng, InverseCumulativeNormal> normal(MersenneTwisterUniformRng(42));

    Real tol = 30.0E-4;

    Real z0 = -3.0;
    while (z0 < 3.01) {
        std::vector<RandomVariable> values(g.size(), RandomVariable(n)), derivatives(g.size(), RandomVariable(n));
        BOOST_TEST_MESSAGE("z0=" << z0 << ":");
        for (Size i = 0; i < n; ++i) {
            values[z].set(i, z0 + normal.next().value);
        }
        forwardEvaluation(g, values, getRandomVariableOps(n));
        Real av = expectation(values[y])[0];
        Real refAv = 1.0 - CumulativeNormalDistribution()(-z0);
        BOOST_TEST_MESSAGE("E( 1_{z>0} ) = " << av << ", reference " << refAv << ", diff " << refAv - av);
        BOOST_CHECK_SMALL(av - refAv, tol);

        derivatives[y] = RandomVariable(n, 1.0);
        backwardDerivatives(g, values, derivatives,
                            getRandomVariableGradients(1, 2, QuantLib::LsmBasisSystem::Monomial, epsilon));
        Real dav = expectation(derivatives[z])[0];
        Real refDav = NormalDistribution()(-z0);
        // it is dz / dz0 = 1, so we are really computing  d/dz0 E(1_{z>0})
        // and we have E(1_{z>0}) = 1 - \Phi(-z0), so d/dz0 E(1_{z>0}) = phi(-z0)
        BOOST_TEST_MESSAGE("E( d/dz 1_{z>0} ) = " << dav << ", reference " << refDav << ", diff " << refDav - dav);
        BOOST_CHECK_SMALL(dav - refDav, tol);

        z0 += 0.5;
    }
}

BOOST_AUTO_TEST_CASE(testPow) {
    BOOST_TEST_MESSAGE("Testing pow function...");

    Real tol = 1E-13;

    for (int p = -300; p <= 300; ++p) {

        ComputationGraph g;
        g.enableLabels();
        auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
        auto y = cg_pow(g, x, cg_const(g, static_cast<double>(p)), "y");

        std::vector<RandomVariable> values(g.size(), RandomVariable(1));

        for (auto const& [v, id] : g.constants())
            values[id] = RandomVariable(1, v);

        Real v = 5.0;
        values[x] = RandomVariable(1, v);

        forwardEvaluation(g, values, getRandomVariableOps(1));

        // BOOST_TEST_MESSAGE("SSA Form for p = " << p << ":\n" + ssaForm(g, getRandomVariableOpLabels(), values));

        BOOST_CHECK_CLOSE(values[y].at(0), std::pow(v, p), tol);
    }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()
