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

#include <qle/models/dkimpliedzeroinflationtermstructure.hpp>

using QuantLib::Date;
using QuantLib::Size;
using QuantLib::Time;

namespace QuantExt {

DkImpliedZeroInflationTermStructure::DkImpliedZeroInflationTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index)
    : ZeroInflationModelTermStructure(model, index) {}

QL_DEPRECATED_DISABLE_WARNING
DkImpliedZeroInflationTermStructure::DkImpliedZeroInflationTermStructure(
    const QuantLib::ext::shared_ptr<CrossAssetModel>& model, Size index, bool indexIsInterpolated)
    : ZeroInflationModelTermStructure(model, index, indexIsInterpolated) {}
QL_DEPRECATED_ENABLE_WARNING

Real DkImpliedZeroInflationTermStructure::zeroRateImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "DkImpliedZeroInflationTermStructure::zeroRateImpl: negative time (" << t << ") given");
    auto p = model_->infdkI(index_, relativeTime_, relativeTime_ + t, state_[0], state_[1]);
    return std::pow(p.second, 1 / t) - 1;
}

void DkImpliedZeroInflationTermStructure::checkState() const {
    // For DK, expect the state to be two variables i.e. z_I and y_I.
    QL_REQUIRE(state_.size() == 2, "DkImpliedZeroInflationTermStructure: expected state to have " <<
        "two elements but got " << state_.size());
}

}
