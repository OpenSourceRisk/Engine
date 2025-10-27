/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/cashflows/interpolatediborcouponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

InterpolatedIborCoupon::InterpolatedIborCoupon(const Date& paymentDate, const Real nominal, const Date& accrualStart,
                                               const Date& accrualEnd, const Size fixingDays,
                                               const QuantLib::ext::shared_ptr<InterpolatedIborIndex>& index, Real gearing,
                                               Spread spread, const Date& refPeriodStart, const Date& refPeriodEnd,
                                               const DayCounter& dayCounter, bool isInArrears,
                                               const Date& exCouponDate,
                                               const QuantLib::ext::shared_ptr<IborIndex>& iborIndex)
    : FloatingRateCoupon(paymentDate, nominal, accrualStart, accrualEnd, fixingDays,
                         index, gearing, spread,
                         refPeriodStart, refPeriodEnd, dayCounter, isInArrears, exCouponDate),
      interpolatedIborIndex_(index), iborIndex_(iborIndex) {
    fixingDate_ = FloatingRateCoupon::fixingDate();
}

void InterpolatedIborCoupon::initializeCachedData() const {
    auto p = ext::dynamic_pointer_cast<InterpolatedIborCouponPricer>(pricer_);
    QL_REQUIRE(p, "InterpolatedIborCoupon: pricer not set or not derived from InterpolatedIborCouponPricer");
    p->initializeCachedData(*this);
}

void InterpolatedIborCoupon::accept(AcyclicVisitor& v) {
    Visitor<InterpolatedIborCoupon>* v1 = dynamic_cast<Visitor<InterpolatedIborCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        FloatingRateCoupon::accept(v);
}

} // namespace QuantExt
