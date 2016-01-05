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

PiecewiseConstantHelper1::PiecewiseConstantHelper1(const Array &t)

    : t_(t), y_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t);
}

PiecewiseConstantHelper2::PiecewiseConstantHelper2(const Array &t)
    : zeroCutoff_(1.0E-6), t_(t),
      y_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t);
}

PiecewiseConstantHelper3::PiecewiseConstantHelper3(const Array &t)
    : zeroCutoff_(1.0E-6), t_(t),
      y1_(boost::make_shared<PseudoParameter>(t.size() + 1)),
      y2_(boost::make_shared<PseudoParameter>(t.size() + 1)) {
    checkTimes(t);
}

} // namespace QuantExt
