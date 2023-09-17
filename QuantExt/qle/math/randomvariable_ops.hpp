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

#include <map>

namespace QuantExt {

// random variable operations

using RandomVariableOp = std::function<RandomVariable(const std::vector<const RandomVariable*>&)>;

// basisFn maps possible state sizes to sets of appropriate basis functions
std::vector<RandomVariableOp> getRandomVariableOps(
    const Size size,
    const std::map<Size, std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>>&
        basisFn = {});

// random variable gradients

using RandomVariableGrad =
    std::function<std::vector<RandomVariable>(const std::vector<const RandomVariable*>&, const RandomVariable*)>;
std::vector<RandomVariableGrad> getRandomVariableGradients(
    const Size size, const double eps = 0.2,
    const std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>& basisFn = {});

// random variable flags which values are needed to compute the gradient

using RandomVariableOpNodeRequirements = std::function<std::pair<std::vector<bool>, bool>(const std::size_t)>;
std::vector<RandomVariableOpNodeRequirements> getRandomVariableOpNodeRequirements();

} // namespace QuantExt
