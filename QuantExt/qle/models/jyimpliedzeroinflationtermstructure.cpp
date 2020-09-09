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

#include <qle/models/jyimpliedzeroinflationtermstructure.hpp>

using QuantLib::Date;
using QuantLib::Size;
using QuantLib::Time;

namespace QuantExt {

JyImpliedZeroInflationTermStructure::JyImpliedZeroInflationTermStructure(
    const boost::shared_ptr<CrossAssetModel>& model, Size index)
    : ZeroInflationModelTermStructure(model, index) {}

Real JyImpliedZeroInflationTermStructure::zeroRateImpl(Time t) const {
    QL_REQUIRE(t >= 0.0, "JyImpliedZeroInflationTermStructure::zeroRateImpl: negative time (" << t << ") given");
    QL_FAIL("Not implemented yet.");
    // auto p = model_->infdkI(index_, relativeTime_, relativeTime_ + t, state_[0], state_[1]);
    // return std::pow(p.second, 1 / t) - 1;
}

void JyImpliedZeroInflationTermStructure::checkState() const {
    // For JY, expect the state to be three variables i.e. z_I, c_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "JyImpliedZeroInflationTermStructure: expected state to have " <<
        "three elements but got " << state_.size());
}

}
