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
                    const std::vector<double>& rates,
                    const std::vector<double>& notionals,
                    const Schedule& schedule,
                    const DayCounter& dc,
                    const BusinessDayConvention& bdc,
                    const Compounding& comp,
                    bool subtractNotional) :
                    Coupon(schedule.calendar().adjust(periodEnd, bdc), notionals.back(), periodStart, periodEnd),
                    periodStart_(periodStart), periodEnd_(periodEnd), rates_(rates),
                    notionals_(notionals), schedule_(schedule), dc_(dc), bdc_(bdc),
                    comp_(comp), subtractNotional_(subtractNotional)
                    {

                        QL_REQUIRE(comp_ == QuantLib::Compounded || comp_ == QuantLib::Simple,
                            "Compounding method " << comp_ << " not supported");

                        // we loop over the dates in the schedule, computing the compound factor.
                        // For the Compounded rule:
                        // (1+r)^dcf_0 *  (1+r)^dcf_1 * ... = (1+r)^(dcf_0 + dcf_1 + ...)
                        // So we compute the sum of all DayCountFractions in the loop.
                        // For the Simple rule:
                        // (1 + r * dcf_0) * (1 + r * dcf_1)...

                        std::vector<Date> dates = schedule_.dates();

                        double totalDCF = 0.0;
                        double compoundFactor = 1.0;

                        nominal_ = 0.0;
                        rate_ = 0.0;
                        amount_ = 0.0;

                        for (Size i = 0; i < dates.size() - 1; i++) {

                            Date startDate = dates[i];
                            Date endDate = dates[i+1];

                            if(startDate > periodStart_ || endDate > periodEnd_)
                                break;

                            double dcf = dc_.yearFraction(startDate, endDate);
                            nominal_ = i < notionals_.size() ? notionals_[i] : notionals_.back();
                            rate_ = i < rates_.size() ? rates_[i] : rates_.back();

                            if (comp_ == QuantLib::Simple){
                                compoundFactor *= (1 + rate_ * dcf);
                            } else {
                                totalDCF += dcf;
                            }
                            if (comp_ == QuantLib::Compounded)
                                compoundFactor = pow(1.0 + rate_, totalDCF);

                            double fixedAmount = nominal_;

                            if (subtractNotional_)
                                fixedAmount *= (compoundFactor - 1);
                            else
                                fixedAmount *= compoundFactor;

                            amount_ = fixedAmount;

                        };

                    }//ctor

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

    std::vector<Date> dates = schedule_.dates();

    double totalDCF = 0.0;
    double compoundFactor = 1.0;
    double fixedAmount = 0.0;

    for (Size i = 0; i < dates.size() - 1; i++) {

        Date startDate = dates[i];
        Date endDate = dates[i+1];

        if(startDate > appliedPeriodEnd)
            break;

        if(endDate > appliedPeriodEnd)
            endDate = appliedPeriodEnd;


        double dcf = dc_.yearFraction(startDate, endDate);
        fixedAmount = i < notionals_.size() ? notionals_[i] : notionals_.back();
        double fixedRate = i < rates_.size() ? rates_[i] : rates_.back();

        if (comp_ == QuantLib::Simple){
            compoundFactor *= (1 + fixedRate * dcf);
        } else {
            totalDCF += dcf;
        }
        if (comp_ == QuantLib::Compounded)
            compoundFactor = pow(1.0 + fixedRate, totalDCF);

        if (subtractNotional_)
            fixedAmount *= (compoundFactor - 1);
        else
            fixedAmount *= compoundFactor;

    }

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
