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

/*! \file qle/math/randomvariable_ops.hpp
    \brief ops for type randomvariable
*/

#pragma once

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_opcodes.hpp>

#include <ql/methods/montecarlo/lsmbasissystem.hpp>

#include <map>

namespace QuantExt {

// random variable operations

using RandomVariableOp = std::function<RandomVariable(const std::vector<const RandomVariable*>&)>;

// eps determines the smoothing, 0 means no smoothing (default)
std::vector<RandomVariableOp>
getRandomVariableOps(const Size size, const Size regressionOrder = 2,
                     const QuantLib::LsmBasisSystem::PolynomialType polynomType = QuantLib::LsmBasisSystem::Monomial,
                     const double eps = 0.0, QuantLib::Real regressionVarianceCutoff = Null<Real>());

// random variable gradients

using RandomVariableGrad =
    std::function<std::vector<RandomVariable>(const std::vector<const RandomVariable*>&, const RandomVariable*)>;

// eps determines the smoothing, 0 means no smoothing
std::vector<RandomVariableGrad> getRandomVariableGradients(
    const Size size, const Size regressionOrder = 2,
    const QuantLib::LsmBasisSystem::PolynomialType polynomType = QuantLib::LsmBasisSystem::Monomial,
    const double eps = 0.2, QuantLib::Real regressionVarianceCutoff = Null<Real>());

// random variable flags which values are needed to compute the gradient

using RandomVariableOpNodeRequirements = std::function<std::pair<std::vector<bool>, bool>(const std::size_t)>;
std::vector<RandomVariableOpNodeRequirements> getRandomVariableOpNodeRequirements();

std::vector<bool> getRandomVariableOpAllowsPredeletion();

} // namespace QuantExt
