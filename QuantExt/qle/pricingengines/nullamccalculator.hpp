/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file nullamccalculator.hpp
    \brief amc calculator that returns zero results
*/

#pragma once

#include <qle/pricingengines/amccalculator.hpp>

namespace QuantExt {

class NullAmcCalculator : public AmcCalculator {
public:
    QuantLib::Currency npvCurrency() override;

    std::vector<QuantExt::RandomVariable> simulatePath(const std::vector<QuantLib::Real>& pathTimes,
                                                       const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
                                                       const std::vector<size_t>& relevantPathIndex,
                                                       const std::vector<size_t>& relevantTimeIndex) override;
};

} // namespace QuantExt
