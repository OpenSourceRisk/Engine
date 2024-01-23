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
    double sum = 0.0, n = static_cast<Real>(end1 - begin1);
    auto w = begin2;
    for (auto v = begin1; v != end1; ++v, ++w) {
        auto w2 = begin2;
        for (auto v2 = begin1; v2 != v; ++v2, ++w2) {
            Real t = (*v2 - *v) * (*w2 - *w);
            if (!QuantLib::close_enough(t, 0.0)) {
                sum += t > 0.0 ? 1.0 : -1.0;
            }
        }
    }
    return 2.0 * sum / (n * (n - 1));
}

} // namespace QuantExt
