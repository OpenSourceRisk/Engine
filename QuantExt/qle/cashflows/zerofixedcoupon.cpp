/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/cashflows/zerofixedcoupon.hpp>
#include <iostream>
namespace QuantExt {

 ZeroFixedCoupon::ZeroFixedCoupon(const Date& periodStart,
                                    const Date& periodEnd,
                                    const Date& paymentDate,
                                    double nominal,
                                    double rate,
                                    double amount,
                                    double currentAccrual,
                                    const DayCounter& dc,
                                    Compounding comp,
                                    bool subtractNotional) :
                    Coupon(paymentDate, nominal, periodStart, periodEnd),
                    periodStart_(periodStart), periodEnd_(periodEnd), paymentDate_(paymentDate),
                    nominal_(nominal), rate_(rate), amount_(amount), currentAccrual_(currentAccrual),
                    dc_(dc), comp_(comp), subtractNotional_(subtractNotional)
                    {

                        QL_REQUIRE(comp_ == QuantLib::Compounded || comp_ == QuantLib::Simple,
                            "Compounding method " << comp_ << " not supported");

                    };

Real ZeroFixedCoupon::amount() const { return amount_; }

Real ZeroFixedCoupon::nominal() const { return nominal_; }

Real ZeroFixedCoupon::rate() const { return rate_; }

DayCounter ZeroFixedCoupon::dayCounter() const { return dc_; }

Real ZeroFixedCoupon::accruedAmount(const Date& accrualEnd) const {

    //outside this period, return zero
    if(periodStart_ > accrualEnd || periodEnd_ < accrualEnd)
        return 0.0;

    Date appliedPeriodEnd = periodEnd_;

    if(appliedPeriodEnd > accrualEnd)
        appliedPeriodEnd = accrualEnd;

    double dcf = dc_.yearFraction(periodStart_, appliedPeriodEnd);
    double compoundFactor = 1.0;

    if (comp_ == QuantLib::Simple)
        compoundFactor = (1.0 + rate_ * dcf) * currentAccrual_; //currentAccrual_ used as a (previous) compound factor

    if (comp_ == QuantLib::Compounded)
        compoundFactor = pow(1.0 + rate_, currentAccrual_ + dcf); //currentAccrual_ used as a (previous) daycount fraction

    if (subtractNotional_)
        compoundFactor -= 1.0;

    return nominal_ * compoundFactor;

}



void ZeroFixedCoupon::accept(AcyclicVisitor& v) {
    Visitor<ZeroFixedCoupon>* v1 = dynamic_cast<Visitor<ZeroFixedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

} // namespace QuantExt
