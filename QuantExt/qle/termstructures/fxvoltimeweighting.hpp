/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file fxvoltimeweighting.hpp
    \brief helper class to compute weights for fx vol time interpolation
*/

#pragma once

#include <ql/math/interpolation.hpp>

namespace QuantExt {

using namespace QuantLib;

class FxVolatilityTimeWeighting {
public:
    FxVolatilityTimeWeighting(const Date& asof, const DayCounter& dayCounter, const std::vector<double>& weekdayWeights,
                              const std::vector<std::pair<Calendar, double>>& tradingCenters,
                              const std::map<Date, double>& events);
    Real operator()(const double t) const;
    Real operator()(const Date& d) const;

private:
    void update(const double t) const;

    Date asof_;
    DayCounter dayCounter_;
    std::vector<double> weekdayWeights_;
    std::vector<std::pair<Calendar, double>> tradingCenters_;
    std::map<Date, double> events_;

    mutable Date maxDate_;
    mutable Real maxTime_ = -1.0;
    mutable Real lastSlope_ = -1.0;

    mutable std::unique_ptr<Interpolation> w_;
    mutable std::vector<double> x_, y_;
};

} // namespace QuantExt
