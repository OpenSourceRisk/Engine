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

#include <qle/utilities/cashflows.hpp>
#include <iostream>

using QuantLib::Date;
using QuantLib::Rate;
using QuantLib::Real;
using QuantLib::Time;

namespace QuantExt {

Real getOisAtmLevel(const QuantLib::ext::shared_ptr<OvernightIndex>& on, const Date& fixingDate,
                    const Period& rateComputationPeriod) {
    Date today = Settings::instance().evaluationDate();
    Date start = on->valueDate(fixingDate);
    Date end = on->fixingCalendar().advance(start, rateComputationPeriod);
    Date adjStart = std::max(start, today);
    Date adjEnd = std::max(adjStart + 1, end);
    OvernightIndexedCoupon cpn(end, 1.0, adjStart, adjEnd, on);
    cpn.setPricer(QuantLib::ext::make_shared<OvernightIndexedCouponPricer>());
    return cpn.rate();
}

Real getBMAAtmLevel(const QuantLib::ext::shared_ptr<BMAIndex>& bma, const Date& fixingDate,
                    const Period& rateComputationPeriod) {
    Date today = Settings::instance().evaluationDate();
    Date start = bma->fixingCalendar().advance(fixingDate, 1 * Days);
    Date end = bma->fixingCalendar().advance(start, rateComputationPeriod);
    Date adjStart = std::max(start, today);
    Date adjEnd = std::max(adjStart + 1, end);
    AverageBMACoupon cpn(end, 1.0, adjStart, adjEnd, bma);
    return cpn.rate();
}

} // namespace QuantExt
