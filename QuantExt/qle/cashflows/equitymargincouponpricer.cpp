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

#include <qle/cashflows/equitymargincouponpricer.hpp>

namespace QuantExt {

Rate EquityMarginCouponPricer::rate() const {
    
    Calendar calendar = equityCurve_->fixingCalendar();
    
    Date startDate = coupon_->fixingStartDate();
    // the final date in the period is treated differently
    Date endDate = equityCurve_->fixingCalendar().advance(coupon_->fixingEndDate(), -1 * Days);

    Real equityPrice = equityCurve_->fixing(endDate, false, false);
    Real dividends = 0.0;
    Real fx = fxIndex_ ? fxIndex_->fixing(coupon_->fixingEndDate()) : 1.0;
    // Dividends that are already fixed dividends + yield accrued over remaining period.
    // yield accrued = Forward without dividend yield - Forward with dividend yield
    if (isTotalReturn_) {
        // projected dividends from today until the fixing end date
        dividends = equityCurve_->fixing(endDate, false, true) -
                    equityCurve_->fixing(endDate, false, false);
        // subtract projected dividends from today until the fixing start date
        if (coupon_->fixingStartDate() > Settings::instance().evaluationDate()) {
            dividends -= (equityCurve_->fixing(startDate, false, true) -
                          equityCurve_->fixing(startDate, false, false));
        }
        // add historical dividends
        dividends += equityCurve_->dividendsBetweenDates(startDate, endDate);
    }
    
    Real rate = (equityPrice + dividends * dividendFactor_) * fx * fixedRate_.dayCounter().yearFraction(startDate, endDate) * marginFactor_;
    
    // on the valuation date we return the initialPrice
    startDate = endDate;
    endDate = coupon_->fixingEndDate();
    fx = fxIndex_ ? fxIndex_->fixing(coupon_->fixingEndDate()) : 1.0;
    rate += initialPrice_ * fx * fixedRate_.dayCounter().yearFraction(startDate, endDate) * marginFactor_;

    return rate * fixedRate_;
}

void EquityMarginCouponPricer::initialize(const EquityMarginCoupon& coupon) {

    coupon_ = &coupon;

    marginFactor_ = coupon.marginFactor();
    fixedRate_ = coupon.fixedRate();
    equityCurve_ = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(coupon.equityCurve());
    fxIndex_ = QuantLib::ext::dynamic_pointer_cast<FxIndex>(coupon.fxIndex());
    isTotalReturn_ = coupon.isTotalReturn();
    dividendFactor_ = coupon.dividendFactor();
    initialPrice_ = coupon.initialPrice();
}

} // namespace QuantExt
