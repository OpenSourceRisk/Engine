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

#include <qle/models/jyimpliedyoyinflationtermstructure.hpp>
#include <ql/time/schedule.hpp>

using QuantLib::Date;
using QuantLib::Real;
using QuantLib::MakeSchedule;
using QuantLib::Schedule;
using QuantLib::Size;
using QuantLib::Time;
using std::map;
using std::vector;

namespace QuantExt {

JyImpliedYoYInflationTermStructure::JyImpliedYoYInflationTermStructure(
    const boost::shared_ptr<CrossAssetModel>& model, Size index)
    : YoYInflationModelTermStructure(model, index) {}

map<Date, Real> JyImpliedYoYInflationTermStructure::yoyRates(const vector<Date>& dts, const Period& obsLag) const {
    QL_FAIL("Not implemented yet.");
}

Real JyImpliedYoYInflationTermStructure::yoySwapletRate(Time S, Time T) const {
    QL_FAIL("Not implemented yet. Maybe not needed.");
}

void JyImpliedYoYInflationTermStructure::checkState() const {
    // For JY YoY, expect the state to be three variables i.e. z_I and c_I and z_{ir}.
    QL_REQUIRE(state_.size() == 3, "JyImpliedYoYInflationTermStructure: expected state to have " <<
        "three elements but got " << state_.size());
}

}
