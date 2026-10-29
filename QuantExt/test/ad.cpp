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

namespace {
// value and both partial derivatives of out = f(x, y) at (x0, y0), by forward evaluation and backward derivatives
struct ValueAndGradient {
    Real value, dx, dy;
};

template <typename F> ValueAndGradient valueAndGradient(F f, Real x0, Real y0, Real eps) {
    ComputationGraph g;
    auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
    auto y = cg_var(g, "y", ComputationGraph::VarDoesntExist::Create);
    auto out = f(g, x, y);
    std::vector<RandomVariable> values(g.size(), RandomVariable(1, 0.0)), derivatives(g.size(), RandomVariable(1, 0.0));
    values[x] = RandomVariable(1, x0);
    values[y] = RandomVariable(1, y0);
    forwardEvaluation(g, values, getRandomVariableOps(1, 2, QuantLib::LsmBasisSystem::Monomial, eps));
    derivatives[out] = RandomVariable(1, 1.0);
    backwardDerivatives(g, values, derivatives,
                        getRandomVariableGradients(1, 2, QuantLib::LsmBasisSystem::Monomial, eps));
    return {values[out][0], derivatives[x][0], derivatives[y][0]};
}
} // namespace

BOOST_AUTO_TEST_CASE(testDivisionDerivatives) {
    BOOST_TEST_MESSAGE("Testing the derivatives of a division...");

    // z = x / y: dz/dx = 1/y, dz/dy = -x/y^2
    auto r = valueAndGradient([](ComputationGraph& g, std::size_t x, std::size_t y) { return cg_div(g, x, y); }, 6.0,
                              3.0, 0.0);
    constexpr Real tol = 1E-14;
    BOOST_CHECK_CLOSE(r.value, 2.0, tol);
    BOOST_CHECK_CLOSE(r.dx, 1.0 / 3.0, tol);
    BOOST_CHECK_CLOSE(r.dy, -6.0 / 9.0, tol);
}

BOOST_AUTO_TEST_CASE(testMinMaxAndIndicatorOperations) {
    BOOST_TEST_MESSAGE("Testing min, max and indicator operations away from the kink, smoothed with eps > 0...");

    // The kink is smoothed over a band of width eps around x = y. Away from it the values and derivatives are those
    // of the exact functions, in both orderings of the arguments.
    constexpr Real eps = 0.05;
    constexpr Real tol = 1E-12;
    auto min = [](ComputationGraph& g, std::size_t x, std::size_t y) { return cg_min(g, x, y); };
    auto max = [](ComputationGraph& g, std::size_t x, std::size_t y) { return cg_max(g, x, y); };
    auto geq = [](ComputationGraph& g, std::size_t x, std::size_t y) { return cg_indicatorGeq(g, x, y); };
    auto gt = [](ComputationGraph& g, std::size_t x, std::size_t y) { return cg_indicatorGt(g, x, y); };

    struct Case {
        Real x, y;
    };
    for (auto const& c : {Case{2.0, 3.0}, Case{3.0, 2.0}}) {
        BOOST_TEST_MESSAGE("x = " << c.x << ", y = " << c.y);
        bool xSmaller = c.x < c.y;

        auto m = valueAndGradient(min, c.x, c.y, eps);
        BOOST_CHECK_CLOSE(m.value, 2.0, tol);
        BOOST_CHECK_SMALL(m.dx - (xSmaller ? 1.0 : 0.0), tol);
        BOOST_CHECK_SMALL(m.dy - (xSmaller ? 0.0 : 1.0), tol);

        auto M = valueAndGradient(max, c.x, c.y, eps);
        BOOST_CHECK_CLOSE(M.value, 3.0, tol);
        BOOST_CHECK_SMALL(M.dx - (xSmaller ? 0.0 : 1.0), tol);
        BOOST_CHECK_SMALL(M.dy - (xSmaller ? 1.0 : 0.0), tol);

        auto ge = valueAndGradient(geq, c.x, c.y, eps);
        BOOST_CHECK_SMALL(ge.value - (xSmaller ? 0.0 : 1.0), tol);
        BOOST_CHECK_SMALL(ge.dx, tol);
        BOOST_CHECK_SMALL(ge.dy, tol);

        auto g = valueAndGradient(gt, c.x, c.y, eps);
        BOOST_CHECK_SMALL(g.value - (xSmaller ? 0.0 : 1.0), tol);
        BOOST_CHECK_SMALL(g.dx, tol);
        BOOST_CHECK_SMALL(g.dy, tol);
    }
}

BOOST_AUTO_TEST_CASE(testMinMaxGradientsInsideTheSmoothingBand) {
    BOOST_TEST_MESSAGE("Testing min and max gradients for samples inside the smoothing band...");

    // Samples around the kink x = y. The derivative of the smoothed step is a logistic density of width delta, with
    // delta = rms(x - y) * eps / 2. For the smoothed max(x, y) = y + (x - y) * step(x - y) the partial derivatives are
    //   d/dx = step'(d) * d + 1{d >= 0},    d/dy = step'(d) * (-d) + 1{d <= 0},    d = x - y,
    // and for min(x, y) the roles of x and y are exchanged.
    constexpr Real eps = 0.5;
    constexpr Real tol = 1E-10;
    const std::vector<Real> offsets = {-0.6, -0.3, -0.1, 0.0, 0.1, 0.3, 0.6};
    const Size n = offsets.size();

    Real sumSq = 0.0;
    for (Real o : offsets)
        sumSq += o * o;
    const Real delta = std::sqrt(sumSq / static_cast<Real>(n)) * eps / 2.0;
    auto stepDerivative = [delta](Real d) { return 1.0 / (delta * (2.0 + std::exp(d / delta) + std::exp(-d / delta))); };
    auto geq = [](Real a, Real b) { return (a >= b || QuantLib::close_enough(a, b)) ? 1.0 : 0.0; };

    for (bool isMax : {true, false}) {
        ComputationGraph g;
        auto x = cg_var(g, "x", ComputationGraph::VarDoesntExist::Create);
        auto y = cg_var(g, "y", ComputationGraph::VarDoesntExist::Create);
        auto out = isMax ? cg_max(g, x, y) : cg_min(g, x, y);

        std::vector<RandomVariable> values(g.size(), RandomVariable(n, 0.0)),
            derivatives(g.size(), RandomVariable(n, 0.0));
        values[x] = RandomVariable(n, 0.0);
        values[y] = RandomVariable(n, 0.0);
        for (Size i = 0; i < n; ++i) {
            values[x].set(i, 1.0 + offsets[i]);
            values[y].set(i, 1.0);
        }
        forwardEvaluation(g, values, getRandomVariableOps(n, 2, QuantLib::LsmBasisSystem::Monomial, eps));
        derivatives[out] = RandomVariable(n, 1.0);
        backwardDerivatives(g, values, derivatives,
                            getRandomVariableGradients(n, 2, QuantLib::LsmBasisSystem::Monomial, eps));

        for (Size i = 0; i < n; ++i) {
            Real xi = 1.0 + offsets[i], yi = 1.0, d = xi - yi;
            Real expectedDx = isMax ? stepDerivative(d) * d + geq(xi, yi) : stepDerivative(d) * (-d) + geq(yi, xi);
            Real expectedDy = isMax ? stepDerivative(d) * (-d) + geq(yi, xi) : stepDerivative(d) * d + geq(xi, yi);
            BOOST_CHECK_SMALL(derivatives[x][i] - expectedDx, tol);
            BOOST_CHECK_SMALL(derivatives[y][i] - expectedDy, tol);
        }
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
