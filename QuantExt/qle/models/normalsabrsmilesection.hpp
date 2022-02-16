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

/*! \file normalsabrsmilesection.hpp
    \brief normal sabr smile section class
*/

#pragma once

#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <vector>

namespace QuantExt {
using namespace QuantLib;

class NormalSabrSmileSection : public SmileSection {
public:
    NormalSabrSmileSection(Time timeToExpiry, Rate forward, const std::vector<Real>& sabrParameters);
    NormalSabrSmileSection(const Date& d, Rate forward, const std::vector<Real>& sabrParameters,
                           const DayCounter& dc = Actual365Fixed());
    Real minStrike() const override { return -QL_MAX_REAL; }
    Real maxStrike() const override { return QL_MAX_REAL; }
    Real atmLevel() const override { return forward_; }

protected:
    Real varianceImpl(Rate strike) const override;
    Volatility volatilityImpl(Rate strike) const override;

private:
    Real alpha_, nu_, rho_, forward_;
};

} // namespace QuantExt
