/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/models/piecewiseconstanthelper.hpp>

namespace QuantExt {

namespace {
void checkTimes(const Array &t) {
    if (t.size() == 0)
        return;
    QL_REQUIRE(t.front() > 0.0, "first time (" << t.front()
                                               << ") must be positive");
    for (Size i = 0; i < t.size() - 1; ++i) {
        QL_REQUIRE(t[i] < t[i + 1],
                   "times must be strictly increasing, entries at ("
                       << i << "," << i + 1 << ") are (" << t[i] << ","
                       << t[i + 1]);
    }
}
}

PiecewiseConstantHelper1::PiecewiseConstantHelper1(const Array &t,
                                                   const Array &y)
    : t_(t), y_(y) {
    QL_REQUIRE(t_.size() + 1 == y_.size(),
               "t size (" << t_.size() << ") + 1 = " << t_.size() + 1
                          << " must be equal to y size (" << y_.size() << ")");
    checkTimes(t);
    update();
}

PiecewiseConstantHelper2::PiecewiseConstantHelper2(const Array &t,
                                                   const Array &y)
    : zeroCutoff_(1.0E-6), t_(t), y_(y) {
    QL_REQUIRE(t_.size() + 1 == y_.size(),
               "t size (" << t_.size() << ") + 1 = " << t_.size() + 1
                          << " must be equal to y size (" << y_.size() << ")");
    checkTimes(t);
    update();
}

PiecewiseConstantHelper3::PiecewiseConstantHelper3(const Array &t,
                                                   const Array &y1,
                                                   const Array &y2)
    : zeroCutoff_(1.0E-6), t_(t), y1_(y1), y2_(y2) {
    QL_REQUIRE(t_.size() + 1 == y1_.size(),
               "t size (" << t_.size() << ") + 1 = " << t_.size() + 1
                          << " must be equal to y1 size (" << y1_.size()
                          << ")");
    QL_REQUIRE(y1_.size() == y2_.size(),
               "y1 size (" << y1_.size() << ") must be equal to y2 size ("
                           << y2_.size() << ")");
    checkTimes(t);
    update();
}

} // namespace QuantExt
