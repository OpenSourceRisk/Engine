/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <iostream>
#include <qle/cashflows/iborfracoupon.hpp>
namespace QuantExt {

IborFraCoupon::IborFraCoupon(const QuantLib::Date& startDate, const QuantLib::Date& endDate, QuantLib::Real nominal,
                             const QuantLib::ext::shared_ptr<QuantLib::IborIndex>& index, const double strikeRate)
    : QuantLib::IborCoupon(startDate, nominal, startDate,
                           index->fixingCalendar().adjust(endDate, index->businessDayConvention()),
                           index->fixingDate(startDate), index, 1.0, -strikeRate) {}

QuantLib::Real IborFraCoupon::amount() const {
    return QuantLib::IborCoupon::amount() /
           (1 + QuantLib::IborCoupon::accrualPeriod() * QuantLib::IborCoupon::indexFixing());
}

} // namespace QuantExt