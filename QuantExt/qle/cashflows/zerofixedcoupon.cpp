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

 ZeroFixedCoupon::ZeroFixedCoupon(  const Date& paymentDate,
                                    double notional,
                                    double rate,
                                    const DayCounter& dc,
                                    const std::vector<Date>& dates,
                                    const Compounding& comp,
                                    bool subtractNotional) :
                    Coupon(paymentDate, notional, dates.front(), dates.back()),
                    notional_(notional), rate_(rate), dc_(dc), dates_(dates),
                    comp_(comp), subtractNotional_(subtractNotional)
                    {

                        QL_REQUIRE(comp_ == QuantLib::Compounded || comp_ == QuantLib::Simple,
                            "Compounding method " << comp_ << " not supported");

                        QL_REQUIRE(dates_.size() >= 2, "Number of schedule dates expected at least 2, got " << dates_.size());

                        amount_ = accruedAmount(dates_.back());

                    }//ctor

Real ZeroFixedCoupon::amount() const { return amount_; }

Real ZeroFixedCoupon::nominal() const { return notional_; }

Real ZeroFixedCoupon::rate() const { return rate_; }

DayCounter ZeroFixedCoupon::dayCounter() const { return dc_; }

Real ZeroFixedCoupon::accruedAmount(const Date& accrualEnd) const {

    //outside this period, return zero
    if(dates_.front() > accrualEnd || dates_.back() < accrualEnd)
        return 0.0;

    double totalDCF = 0.0;
    double compoundFactor = 1.0;

    // we loop over the dates in the schedule, computing the compound factor.
    // For the Compounded rule:
    // (1+r)^dcf_0 *  (1+r)^dcf_1 * ... = (1+r)^(dcf_0 + dcf_1 + ...)
    // So we compute the sum of all DayCountFractions in the loop.
    // For the Simple rule:
    // (1 + r * dcf_0) * (1 + r * dcf_1)...

    for (Size i = 0; i < dates_.size() - 1; i++) {

        Date startDate = dates_[i];
        Date endDate = dates_[i+1];

        if(startDate > accrualEnd)
            break;

        if(endDate > accrualEnd)
            endDate = accrualEnd;

        double dcf = dc_.yearFraction(startDate, endDate);

        if (comp_ == QuantLib::Simple)
            compoundFactor *= (1 + rate_ * dcf);

        totalDCF += dcf;
    }

    if (comp_ == QuantLib::Compounded)
        compoundFactor = pow(1.0 + rate_, totalDCF);

    return notional_ * (subtractNotional_ ? compoundFactor - 1.0 : compoundFactor);



}



void ZeroFixedCoupon::accept(AcyclicVisitor& v) {
    Visitor<ZeroFixedCoupon>* v1 = dynamic_cast<Visitor<ZeroFixedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

} // namespace QuantExt
