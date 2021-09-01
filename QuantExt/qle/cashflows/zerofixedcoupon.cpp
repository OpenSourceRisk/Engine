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

namespace QuantExt {

 ZeroFixedCoupon::ZeroFixedCoupon(Real nominal,
                    const Schedule& schedule,
                    const DayCounter& dc,
                    const BusinessDayConvention& bdc,
                    Real fixedRate,
                    Compounding comp,
                    bool subtractNotional) :
                    Coupon(schedule.calendar().adjust(schedule.endDate(), bdc), nominal, schedule.startDate(), schedule.endDate()),
                    nominal_(nominal), fixedRate_(fixedRate), comp_(comp), schedule_(schedule), dc_(dc), subtractNotional_(subtractNotional){

                        QL_REQUIRE(comp_ == QuantLib::Compounded || comp_ == QuantLib::Simple,
                            "Compounding method " << comp_ << " not supported");

                    };

Real ZeroFixedCoupon::amount() const { return accruedAmount(schedule_.endDate()); }

Real ZeroFixedCoupon::nominal() const { return nominal_; }

Real ZeroFixedCoupon::rate() const { return fixedRate_; }

DayCounter ZeroFixedCoupon::dayCounter() const { return dc_; }

Real ZeroFixedCoupon::accruedAmount(const Date& accrualEnd) const {

    Real fixedAmount = nominal_;
    std::vector<Date> dates = schedule_.dates();

    // we loop over the dates in the schedule, computing the compound factor.
    // For the Compounded rule:
    // (1+r)^dcf_0 *  (1+r)^dcf_1 * ... = (1+r)^(dcf_0 + dcf_1 + ...)
    // So we compute the sum of all DayCountFractions in the loop.
    // For the Simple rule:
    // (1 + r * dcf_0) * (1 + r * dcf_1)...

    double totalDCF = 0.0;
    double compoundFactor = 1.0;
    for (Size i = 0; i < dates.size() - 1; i++) {

        Date periodStart = dates[i];
        if(periodStart > accrualEnd)
            continue;

        Date periodEnd = dates[i+1];
        if(periodEnd > accrualEnd)
            periodEnd = accrualEnd;

        double dcf = dc_.yearFraction(periodStart, periodEnd);

        if (comp_ == QuantLib::Simple)
            compoundFactor *= (1 + fixedRate_ * dcf);
        else
            totalDCF += dcf;
    }
    if (comp_ == QuantLib::Compounded)
        compoundFactor = pow(1.0 + fixedRate_, totalDCF);

    if (subtractNotional_)
        fixedAmount *= (compoundFactor - 1);
    else
        fixedAmount *= compoundFactor;

    return fixedAmount;

}



void ZeroFixedCoupon::accept(AcyclicVisitor& v) {
    Visitor<ZeroFixedCoupon>* v1 = dynamic_cast<Visitor<ZeroFixedCoupon>*>(&v);
    if (v1 != 0)
        v1->visit(*this);
    else
        Coupon::accept(v);
}

} // namespace QuantExt
