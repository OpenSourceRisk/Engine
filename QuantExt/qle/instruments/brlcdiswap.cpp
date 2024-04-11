/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

#include <qle/instruments/brlcdiswap.hpp>

#include <ql/cashflows/overnightindexedcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/time/daycounters/business252.hpp>
#include <ql/time/schedule.hpp>
#include <qle/cashflows/brlcdicouponpricer.hpp>
#include <qle/cashflows/couponpricer.hpp>

using namespace QuantLib;
using std::pow;
using std::vector;

namespace QuantExt {

BRLCdiSwap::BRLCdiSwap(Type type, Real nominal, const Date& startDate, const Date& endDate, Rate fixedRate,
                       const QuantLib::ext::shared_ptr<BRLCdi>& overnightIndex, Spread spread, bool telescopicValueDates)
    : OvernightIndexedSwap(type, nominal,
                           Schedule({startDate, endDate}, NullCalendar(),
                                    QuantLib::Unadjusted, QuantLib::Unadjusted, 100 * Years),
                           fixedRate, overnightIndex->dayCounter(), overnightIndex, spread, 0, ModifiedFollowing,
                           overnightIndex->fixingCalendar(), telescopicValueDates),
      startDate_(startDate), endDate_(endDate), index_(overnightIndex) {

    // Need to overwrite the fixed leg with the correct fixed leg for a standard BRL CDI swap
    // Fixed leg is of the form: N [(1 + k) ^ \delta - 1]
    // where \delta is the number of BRL business days in the period divided by 252 i.e.
    // the day count fraction for the period on a Business252 basis.
    Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
    Real fixedLegPayment = nominal * (pow(1.0 + fixedRate, dcf) - 1.0);
    Date paymentDate = legs_[0].back()->date();
    QuantLib::ext::shared_ptr<CashFlow> fixedCashflow = QuantLib::ext::make_shared<SimpleCashFlow>(fixedLegPayment, paymentDate);
    legs_[0].clear();
    legs_[0].push_back(fixedCashflow);
    registerWith(fixedCashflow);

    // Set the pricer on the BRL CDI coupon
    QL_REQUIRE(legs_[1].size() == 1, "BRLCdiSwap expected exactly one overnight coupon");
    QuantLib::ext::shared_ptr<QuantLib::OvernightIndexedCoupon> coupon =
        QuantLib::ext::dynamic_pointer_cast<QuantLib::OvernightIndexedCoupon>(legs_[1][0]);
    QL_REQUIRE(coupon, "BRLCdiSwap: expected QuantLib::OvernightIndexedCoupon");
    coupon->setPricer(QuantLib::ext::make_shared<BRLCdiCouponPricer>());
}

Real BRLCdiSwap::fixedLegBPS() const {

    calculate();

    const Spread basisPoint = 1.0e-4;
    if (!close(endDiscounts_[0], 0.0) && endDiscounts_[0] != Null<DiscountFactor>()) {
        DiscountFactor df = payer_[0] * endDiscounts_[0];
        Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
        legBPS_[0] = df * nominal() * (pow(1.0 + fixedRate() + basisPoint, dcf) - pow(1.0 + fixedRate(), dcf));
        return legBPS_[0];
    }

    QL_FAIL("BRLCdiSwap cannot calculate fixed leg BPS because end discount is not populated");
}

Real BRLCdiSwap::fairRate() const {

    calculate();

    if (!close(endDiscounts_[0], 0.0) && endDiscounts_[0] != Null<DiscountFactor>()) {
        DiscountFactor df = -payer_[0] * endDiscounts_[0];
        Time dcf = index_->dayCounter().yearFraction(startDate_, endDate_);
        return pow(overnightLegNPV() / (nominal() * df) + 1.0, 1.0 / dcf) - 1.0;
    }

    QL_FAIL("BRLCdiSwap cannot calculate fair rate because end discount is not populated");
}

} // namespace QuantExt
