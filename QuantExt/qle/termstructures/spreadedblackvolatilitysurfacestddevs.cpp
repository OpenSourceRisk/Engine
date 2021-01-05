/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <qle/termstructures/spreadedblackvolatilitysurfacestddevs.hpp>

#include <ql/quotes/all.hpp>

#include <cmath>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

SpreadedBlackVolatilitySurfaceStdDevs::SpreadedBlackVolatilitySurfaceStdDevs(
    const Handle<BlackVolTermStructure>& referenceVol, const Handle<Quote>& spot, const std::vector<Time>& times,
    const std::vector<Real>& stdDevs, const std::vector<std::vector<Handle<Quote>>>& volSpreads,
    const boost::shared_ptr<EqFxIndexBase>& index, bool stickyStrike)
    : SpreadedBlackVolatilitySurfaceMoneyness(referenceVol, spot, times, stdDevs, volSpreads, stickyStrike),
      index_(index) {
    if (!stickyStrike) {
        QL_REQUIRE(index_ != nullptr, "index required for vol surface");
        registerWith(index_);
    } else {
        for (Size i = 0; i < times_.size(); i++) {
            Time t = times_[i];
            Real fwd = index_->forecastFixing(t);
            forwards_.push_back(fwd);
        }
        forwardCurve_ = Linear().interpolate(times_.begin(), times_.end(), forwards_.begin());
    }
}

Real SpreadedBlackVolatilitySurfaceStdDevs::moneyness(Time t, Real strike) const {
    if (strike == Null<Real>() || strike == 0) {
        return 0.0;
    } else {
        Real fwd;
        if (stickyStrike_)
            fwd = forwardCurve_(t, true);
        else
            fwd = index_->forecastFixing(t);
        Real num = log(strike / fwd);
        Real denom = referenceVol_->blackVol(t, fwd) * std::sqrt(t);
        return num / denom;
    }
}

} // namespace QuantExt
