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

#include <qle/models/piecewiseconstantparametrization.hpp>

namespace QuantExt {

PiecewiseConstantParametrization::PiecewiseConstantParametrization(
    const Currency &currency, const Array &t1, const Array &y1, const Array &t2,
    const Array &y2)
    : Parametrization(currency), zeroCutoff_(1.0E-6), t1_(t1), y1_(y1), t2_(t2),
      y2_(y2) {

    if (t1_.size() == 0 && y1_.size() == 0) {
        compute1_ = false;
    } else {
        compute1_ = true;
        QL_REQUIRE(t1_.size() + 1 == y1_.size(),
                   "t1 size (" << t1_.size() << ") + 1 = " << t1_.size() + 1
                               << " must be equal to y1 size (" << y1_.size()
                               << ")");
    }

    if (t2_.size() == 0 && y2_.size() == 0) {
        compute2_ = false;
    } else {
        compute2_ = true;
        QL_REQUIRE(t2_.size() + 1 == y2_.size(),
                   "t2 size (" << t2_.size() << ") + 1 = " << t2_.size() + 1
                               << " must be equal to y2 size (" << y2_.size()
                               << ")");
    }

    update();
}

} // namespace QuantExt
