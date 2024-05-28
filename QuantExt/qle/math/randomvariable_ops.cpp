/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <qle/math/randomvariable_ops.hpp>

#include <ql/math/comparison.hpp>

#include <boost/math/distributions/normal.hpp>

namespace QuantExt {

std::vector<RandomVariableOp> getRandomVariableOps(const Size size, const Size regressionOrder,
                                                   QuantLib::LsmBasisSystem::PolynomialType polynomType,
                                                   const double eps, QuantLib::Real regressionVarianceCutoff) {
    std::vector<RandomVariableOp> ops;

    // None = 0
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return RandomVariable(); });

    // Add = 1
    ops.push_back([](const std::vector<const RandomVariable*>& args) {
        return std::accumulate(args.begin(), args.end(), RandomVariable(args.front()->size(), 0.0),
                               [](const RandomVariable x, const RandomVariable* y) { return x + *y; });
    });

    // Subtract = 2
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return *args[0] - (*args[1]); });

    // Negative = 3
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return -(*args[0]); });

    // Mult = 4
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return *args[0] * (*args[1]); });

    // Div = 5
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return *args[0] / (*args[1]); });

    // ConditionalExpectation = 6
    ops.push_back([size, regressionOrder, polynomType,
                   regressionVarianceCutoff](const std::vector<const RandomVariable*>& args) {
        std::vector<const RandomVariable*> regressor;
        for (auto r = std::next(args.begin(), 2); r != args.end(); ++r) {
            if ((*r)->initialised() && !(*r)->deterministic())
                regressor.push_back(*r);
        }
        std::vector<RandomVariable> transformedRegressor;
        Matrix coordinateTransform;
        if (regressionVarianceCutoff != Null<Real>()) {
            coordinateTransform = pcaCoordinateTransform(regressor, regressionVarianceCutoff);
            transformedRegressor = applyCoordinateTransform(regressor, coordinateTransform);
            regressor = vec2vecptr(transformedRegressor);
        }
        if (regressor.empty())
            return expectation(*args[0]);
        else {
            auto tmp = multiPathBasisSystem(regressor.size(), regressionOrder, polynomType, size);
            return conditionalExpectation(*args[0], regressor, tmp, !close_enough(*args[1], RandomVariable(size, 0.0)));
        }
    });

    // IndicatorEq = 7
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return indicatorEq(*args[0], *args[1]); });

    // IndicatorGt = 8
    ops.push_back([eps](const std::vector<const RandomVariable*>& args) {
        return indicatorGt(*args[0], *args[1], 1.0, 0.0, eps);
    });

    // IndicatorGeq = 9
    ops.push_back([eps](const std::vector<const RandomVariable*>& args) {
        return indicatorGeq(*args[0], *args[1], 1.0, 0.0, eps);
    });

    // Min = 10
    if (eps == 0.0) {
        ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::min(*args[0], *args[1]); });
    } else {
        ops.push_back([eps](const std::vector<const RandomVariable*>& args) {
            return indicatorGt(*args[0], *args[1], 1.0, 0.0, eps) * (*args[1] - *args[0]) + *args[0];
        });
    }

    // Max = 11
    if (eps == 0.0) {
        ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::max(*args[0], *args[1]); });
    } else {
        ops.push_back([eps](const std::vector<const RandomVariable*>& args) {
            return indicatorGt(*args[0], *args[1], 1.0, 0.0, eps) * (*args[0] - *args[1]) + *args[1];
        });
    }

    // Abs = 12
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::abs(*args[0]); });

    // Exp = 13
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::exp(*args[0]); });

    // Sqrt = 14
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::sqrt(*args[0]); });

    // Log = 15
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::log(*args[0]); });

    // Pow = 16
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::pow(*args[0], *args[1]); });

    // NormalCdf = 17
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::normalCdf(*args[0]); });

    // NormalPdf = 18
    ops.push_back([](const std::vector<const RandomVariable*>& args) { return QuantExt::normalPdf(*args[0]); });

    return ops;
}

std::vector<RandomVariableGrad> getRandomVariableGradients(const Size size, const Size regressionOrder,
                                                           const QuantLib::LsmBasisSystem::PolynomialType polynomType,
                                                           const double eps, const Real regressionVarianceCutoff) {

    std::vector<RandomVariableGrad> grads;

    // None = 0
    grads.push_back([](const std::vector<const RandomVariable*>& args,
                       const RandomVariable* v) -> std::vector<RandomVariable> { return {RandomVariable()}; });

    // Add = 1
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 1.0), RandomVariable(size, 1.0)};
        });

    // Subtract = 2
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 1.0), RandomVariable(size, -1.0)};
        });

    // Negative = 3
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, -1.0)};
        });

    // Mult = 4
    grads.push_back(
        [](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {*args[1], *args[0]};
        });

    // Div = 5
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 1.0) / *args[1], -*args[0] / (*args[1] * *args[1])};
        });

    // ConditionalExpectation = 6
    grads.push_back(
        [](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            QL_FAIL("gradient of conditional expectation not implemented");
        });

    // IndicatorEq = 7
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 0.0), RandomVariable(size, 0.0)};
        });

    // IndicatorGt = 8
    grads.push_back(
        [eps](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            RandomVariable tmp = indicatorDerivative(*args[0] - *args[1], eps);
            return {tmp, -tmp};
        });

    // IndicatorGeq = 9
    grads.push_back(grads.back());

    // Min = 10
    grads.push_back([eps](const std::vector<const RandomVariable*>& args,
                          const RandomVariable* v) -> std::vector<RandomVariable> {
        return {
            indicatorDerivative(*args[1] - *args[0], eps) * (*args[1] - *args[0]) + indicatorGeq(*args[1], *args[0]),
            indicatorDerivative(*args[0] - *args[1], eps) * (*args[0] - *args[1]) + indicatorGeq(*args[0], *args[1])};
    });

    // Max = 11
    grads.push_back([eps](const std::vector<const RandomVariable*>& args,
                          const RandomVariable* v) -> std::vector<RandomVariable> {
        return {
            indicatorDerivative(*args[0] - *args[1], eps) * (*args[0] - *args[1]) + indicatorGeq(*args[0], *args[1]),
            indicatorDerivative(*args[1] - *args[0], eps) * (*args[1] - *args[0]) + indicatorGeq(*args[1], *args[0])};
    });

    // Abs = 12
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {indicatorGeq(*args[0], RandomVariable(size, 0.0), 1.0, -1.0)};
        });

    // Exp = 13
    grads.push_back([](const std::vector<const RandomVariable*>& args,
                       const RandomVariable* v) -> std::vector<RandomVariable> { return {*v}; });

    // Sqrt = 14
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 0.5) / *v};
        });

    // Log = 15
    grads.push_back(
        [size](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {RandomVariable(size, 1.0) / *args[0]};
        });

    // Pow = 16
    grads.push_back(
        [](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {*args[1] / *args[0] * (*v), QuantExt::log(*args[0]) * (*v)};
        });

    // NormalCdf = 17
    grads.push_back(
        [](const std::vector<const RandomVariable*>& args, const RandomVariable* v) -> std::vector<RandomVariable> {
            return {QuantExt::normalPdf(*args[0])};
        });

    // NormalPdf = 18
    grads.push_back([](const std::vector<const RandomVariable*>& args,
                       const RandomVariable* v) -> std::vector<RandomVariable> { return {-(*args[0]) * *v}; });

    return grads;
}

std::vector<RandomVariableOpNodeRequirements> getRandomVariableOpNodeRequirements() {
    std::vector<RandomVariableOpNodeRequirements> res;

    // None = 0
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), false); });

    // Add = 1
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), false); });

    // Subtract = 1
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), false); });

    // Negative = 3
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), false); });

    // Mult = 4
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Div = 5
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // ConditionalExpectation = 6
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), true); });

    // IndicatorEq = 7
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), false); });

    // IndicatorGt = 8
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // IndicatorGeq = 9
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Min = 10
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Max = 11
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Abs = 12
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Exp = 13
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), true); });

    // Sqrt = 14
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, false), true); });

    // Log = 15
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // Pow = 16
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), true); });

    // NormalCdf = 17
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), false); });

    // NormalCdf = 18
    res.push_back([](const std::size_t nArgs) { return std::make_pair(std::vector<bool>(nArgs, true), true); });

    return res;
}

std::vector<bool> getRandomVariableOpAllowsPredeletion() {
    std::vector<bool> result(19, true);
    result[6] = false; // conditional expectation
    return result;
}

} // namespace QuantExt
