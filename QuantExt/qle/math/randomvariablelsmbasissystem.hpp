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

/*! \file qle/math/randomvariablelsmbasissystem.hpp
    \brief ql utility class for random variables
*/

#pragma once

#include <qle/math/randomvariable.hpp>

#include <ql/qldefines.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>

#include <vector>

namespace QuantExt {

class RandomVariableLsmBasisSystem {
public:
    static std::vector<std::function<RandomVariable(const RandomVariable&)>>
    pathBasisSystem(Size order, QuantLib::LsmBasisSystem::PolynomialType type);

    static std::vector<std::function<RandomVariable(const std::vector<const RandomVariable*>&)>>
    multiPathBasisSystem(Size dim, Size order, QuantLib::LsmBasisSystem::PolynomialType type);

    // return the size of a basis system (or std::numeric_limits<Real>::infinity() if too big)
    static Real size(Size dim, Size order);
};

} // namespace QuantExt
