/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

/*! \file qle/utilities/ratehelpers.hpp
    \brief rate helper related utilities.
*/

#include <qle/utilities/ratehelpers.hpp>

#include <ql/cashflows/iborcoupon.hpp>

namespace QuantExt {

QuantLib::Date determineLatestRelevantDate(const std::vector<QuantLib::Leg>& legs,
                                           const std::vector<bool>& includeIndexEstimationEndDate) {
    QuantLib::Size legNo = 0;
    QuantLib::Date result = QuantLib::Date::minDate();
    for (auto const& l : legs) {
        for (auto const& c : l) {
            if (auto d = QuantLib::ext::dynamic_pointer_cast<QuantLib::Coupon>(c)) {
                result = std::max(result, d->accrualEndDate());
            }
            result = std::max(result, c->date());
            if (includeIndexEstimationEndDate.size() <= legNo || includeIndexEstimationEndDate[legNo]) {
                if (auto d = QuantLib::ext::dynamic_pointer_cast<QuantLib::IborCoupon>(c)) {
                    result = std::max(result, d->fixingEndDate());
                }
            }
        }
        ++legNo;
    }
    return result;
}

QuantLib::Date determinePillarDate(const QuantLib::Pillar::Choice pillarChoice, const QuantLib::Date& maturityDate,
                                   const QuantLib::Date& latestRelevantDate) {
    switch (pillarChoice) {
    case QuantLib::Pillar::MaturityDate:
        return maturityDate;
    case QuantLib::Pillar::LastRelevantDate:
        return latestRelevantDate;
    case QuantLib::Pillar::CustomDate:
        QL_FAIL("determinePillarDate(): CustomDate is not supported currently.");
    default:
        QL_FAIL("determinePillarDate(): unknown pillar choice (code " << static_cast<int>(pillarChoice) << ")");
    }
}

} // namespace QuantExt
