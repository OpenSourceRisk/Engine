/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/time.hpp
    \brief time related utilities.
*/

#pragma once

#include <tuple>

namespace QuantExt {

/*! Given a non-empty container x of distinct and sorted values and a value v return a tuple (m,p,w) s.t.
    w * y[m] + (1-w) * y[p]
    linearly interpolates between points (x[0], y[0]), ..., (x[n], y[n]) and extrapolates flat at x = v.
    It is m = p if and only if v <= x[0] or v >= x[n]. In this case we have w = 1 and
    either m = p = 0 (if v <= [0]) or m = p = n (if v >= x[n]). */
template <typename T> std::tuple<Size, Size, Real> interpolationIndices(const T& x, const Real v) {
    QL_REQUIRE(!x.empty(), "interpolationIndices(x," << v << "): empty x");
    if (x.size() == 1)
        return std::make_tuple(0, 0, 1.0);
    if (v < x.front() || QuantLib::close_enough(v, x.front()))
        return std::make_tuple(0, 0, 1.0);
    if (v > x.back() || QuantLib::close_enough(v, x.back()))
        return std::make_tuple(x.size() - 1, x.size() - 1, 1.0);
    Size index = std::distance(x.begin(), std::upper_bound(x.begin(), x.end(), v, [](Real x1, Real x2) {
                                   return x1 < x2 && !QuantLib::close_enough(x1, x2);
                               }));
    return std::make_tuple(index - 1, index, (x[index] - v) / (x[index] - x[index - 1]));
}

} // namespace QuantExt
