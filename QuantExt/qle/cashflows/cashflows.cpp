/*
 Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
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

#include <ql/cashflows/floatingratecoupon.hpp>

#include <qle/cashflows/cashflows.hpp>

using namespace std;

namespace QuantExt {

Real CashFlows::spreadNpv(const Leg& leg, const YieldTermStructure& discountCurve, bool includeSettlementDateFlows,
                          Date settlementDate, Date npvDate) {

    if (leg.empty())
        return 0.0;

    if (settlementDate == Date())
        settlementDate = Settings::instance().evaluationDate();

    if (npvDate == Date())
        npvDate = settlementDate;

    Real spreadNpv = 0.0;
    for (Size i = 0; i < leg.size(); ++i) {

        QuantLib::ext::shared_ptr<FloatingRateCoupon> floatCoupon = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(leg[i]);

        if (floatCoupon && !floatCoupon->hasOccurred(settlementDate, includeSettlementDateFlows)) {

            spreadNpv += floatCoupon->nominal() * floatCoupon->accrualPeriod() * floatCoupon->spread() *
                         discountCurve.discount(floatCoupon->date());
        }
    }

    return spreadNpv / discountCurve.discount(npvDate);
}

Real CashFlows::sumCashflows(const Leg& leg, const Date& startDate, const Date& endDate) {

    // Empty leg return 0
    if (leg.empty())
        return 0.0;

    // If leg is not empty
    Real sumCashflows = 0.0;
    for (Size i = 0; i < leg.size(); ++i) {
        Date cashflowDate = leg[i]->date();
        if (startDate < cashflowDate && cashflowDate <= endDate)
            sumCashflows += leg[i]->amount();
    }
    return sumCashflows;
}

vector<Rate> CashFlows::couponRates(const Leg& leg) {

    vector<Rate> couponRates;
    // Non-empty leg
    for (Size i = 0; i < leg.size(); ++i) {
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(leg[i]);
        if (coupon)
            couponRates.push_back(coupon->rate());
    }
    return couponRates;
}

vector<Rate> CashFlows::couponDcfRates(const Leg& leg) {

    vector<Rate> couponDcfRates;
    // Non-empty leg
    for (Size i = 0; i < leg.size(); ++i) {
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(leg[i]);
        if (coupon)
            couponDcfRates.push_back(coupon->rate() * coupon->accrualPeriod());
    }
    return couponDcfRates;
}

} // namespace QuantExt
