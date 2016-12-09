/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file qle/cashflows/cpicouponpricer.hpp
    \brief Pricer for cpi coupon
*/

#ifndef quantext_cpi_coupon_pricer_hpp
#define quantext_cpi_coupon_pricer_hpp

#include <ql/cashflows/cpicouponpricer.hpp>

using namespace QuantLib;

namespace QuantExt {

/*! \note Extention of QL CPICouponPricer to handle adjusted Notional coupons
*/
class CPICouponPricer : public QuantLib::CPICouponPricer {
public:
    // redemption dates are lagged, i.e. these are the index fixing dates.
    CPICouponPricer(bool adjustedNotional, const std::vector<Date>& redemptionDates) :
        QuantLib::CPICouponPricer(),
        adjustedNotional_(adjustedNotional),
        redemptionDates_(redemptionDates) {}

protected:
    
    // Just need to overload adjustedFixing()

    Rate adjustedFixing(Rate fixing) const {
        if (fixing == Null<Rate>()) {
            if (adjustedNotional_) {
                // Need last redemption date.
                QL_REQUIRE(redemptionDates_.size() > 0, "Empty Redemption Date vector");

                if (coupon_->fixingDate() < redemptionDates_[0]) {
                    // No redeptions yet.
                    fixing = coupon_->indexFixing() / coupon_->baseCPI();
                } else {
                    Date lastRedemption = redemptionDates_[0];
                    for (Size i = 1; i < redemptionDates_.size(); i++) {
                        if (coupon_->fixingDate() >= redemptionDates_[i])
                            lastRedemption = redemptionDates_[i];
                    }
                    // CPICoupon::indexObservation calls the underlying index, and does
                    // any interpolation we need.
                    fixing = 1+((coupon_->indexFixing() - coupon_->indexObservation(lastRedemption))
                               / coupon_->baseCPI());
                }
            } else {
                // This is just like the base class
                fixing = coupon_->indexFixing() / coupon_->baseCPI();
            }
        }
        return fixing;
    }

private:
    bool adjustedNotional_;
    std::vector<Date> redemptionDates_;
};
}   // namespace QuantExt

#endif
