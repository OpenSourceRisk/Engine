/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file normalsabrsmilesection.hpp
    \brief normal sabr smile section class
*/

#pragma once

#include <ql/termstructures/volatility/smilesection.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <vector>

using namespace QuantLib;

namespace QuantExt {

class NormalSabrSmileSection : public SmileSection {
public:
    NormalSabrSmileSection(Time timeToExpiry, Rate forward, const std::vector<Real>& sabrParameters);
    NormalSabrSmileSection(const Date& d, Rate forward, const std::vector<Real>& sabrParameters,
                           const DayCounter& dc = Actual365Fixed());
    Real minStrike() const { return -QL_MAX_REAL; }
    Real maxStrike() const { return QL_MAX_REAL; }
    Real atmLevel() const { return forward_; }

protected:
    Real varianceImpl(Rate strike) const;
    Volatility volatilityImpl(Rate strike) const;

private:
    Real alpha_, nu_, rho_, forward_;
};

} // namespace QuantExt
