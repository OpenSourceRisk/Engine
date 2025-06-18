/*
 Copyright (C) 2017 Quaternion Risk Management Ltd.
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

/*!
  \file qle/math/kendallrankcorrelation.hpp
  \brief Kendall's rank correlation coefficient computation
 */

#pragma once

#include <ql/math/comparison.hpp>

namespace QuantExt {

template <class I1, class I2> Real kendallRankCorrelation(I1 begin1, I1 end1, I2 begin2) {
    /*
    https://www.sciencedirect.com/science/article/pii/S0047259X01920172/
    tau =  (2/Pi)*arcsin(rho) <=> rho = sin(Pi*Tau/2)
    */ 
    Real concordant = 0.0; Real discordant = 0.0;
    Real ties_x = 0.0; Real ties_y = 0.0;
    auto it1_x = begin1;
    auto it1_y = begin2;
    for (; it1_x != end1; it1_x++, it1_y++) {
        auto it2_x = begin1;
        auto it2_y = begin2;
        for (; it2_x != it1_x; it2_x++, it2_y++) {
            const auto dx = *it1_x - *it2_x;
            const auto dy = *it1_y - *it2_y;
            if (QuantLib::close_enough(dx, 0.0) && QuantLib::close_enough(dy, 0.0)) {
                // Tied in both, ignored
                continue;
            } else if (QuantLib::close_enough(dx, 0.0)) {
                ties_x++;
            }
            else if (QuantLib::close_enough(dy, 0.0)) {
                ties_y++;
            } else if (dx * dy > 0.0) {
                concordant++;
            }
            else if (dx * dy < 0.0) {
                discordant++;
            }
        }
    }
    Real denom = std::sqrt((concordant + discordant + ties_x) * (concordant + discordant + ties_y));
    if (QuantLib::close_enough(denom, 0.0)) {
        return 0.0;
    }
    Real tau = (concordant - discordant) / denom;
    return std::sin(tau * M_PI / 2.0);
}

} // namespace QuantExt
