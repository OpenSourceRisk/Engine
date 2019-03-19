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

#include <qle/instruments/bondoption.hpp>
#include <qle/pricingengines/blackbondoptionengine.hpp>
#include <ql/cashflows/cashflowvectors.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/termstructures/volatility/swaption/swaptionvolstructure.hpp>
#include <ql/termstructures/volatility/swaption/swaptionconstantvol.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

namespace QuantExt {
    using namespace QuantLib;

    BondOption::BondOption(Natural settlementDays,
        const Schedule& schedule,
        const DayCounter& paymentDayCounter,
        const Date& issueDate,
        const CallabilitySchedule& putCallSchedule)
        : Bond(settlementDays, schedule.calendar(), issueDate),
        paymentDayCounter_(paymentDayCounter), putCallSchedule_(putCallSchedule) {

        maturityDate_ = schedule.dates().back();

        if (!putCallSchedule_.empty()) {
            Date finalOptionDate = Date::minDate();
            for (Size i = 0; i<putCallSchedule_.size(); ++i) {
                finalOptionDate = std::max(finalOptionDate,
                    putCallSchedule_[i]->date());
            }
            QL_REQUIRE(finalOptionDate <= maturityDate_,
                "Bond cannot mature before last call/put date");
        }

        // derived classes must set cashflows_ and frequency_
    }


    void BondOption::arguments::validate() const {

        QL_REQUIRE(Bond::arguments::settlementDate != Date(),
            "null settlement date");

        QL_REQUIRE(redemption != Null<Real>(), "null redemption");
        QL_REQUIRE(redemption >= 0.0,
            "positive redemption required: "
            << redemption << " not allowed");

        QL_REQUIRE(callabilityDates.size() == callabilityPrices.size(),
            "different number of callability/puttability dates and prices");
        QL_REQUIRE(couponDates.size() == couponAmounts.size(),
            "different number of coupon dates and amounts");
    }

    FixedRateBondOption::FixedRateBondOption(
        Natural settlementDays,
        Real faceAmount,
        const Schedule& schedule,
        const std::vector<Rate>& coupons,
        const DayCounter& accrualDayCounter,
        BusinessDayConvention paymentConvention,
        Real redemption,
        const Date& issueDate,
        const CallabilitySchedule& putCallSchedule)
        : BondOption(settlementDays, schedule, accrualDayCounter,
            issueDate, putCallSchedule) {

        frequency_ = schedule.tenor().frequency();

        bool isZeroCouponBond = (coupons.size() == 1 && close(coupons[0], 0.0));

        if (!isZeroCouponBond) {
            cashflows_ =
                FixedRateLeg(schedule)
                .withNotionals(faceAmount)
                .withCouponRates(coupons, accrualDayCounter)
                .withPaymentAdjustment(paymentConvention);

            addRedemptionsToCashflows(std::vector<Real>(1, redemption));
        }
        else {
            Date redemptionDate = calendar_.adjust(maturityDate_,
                paymentConvention);
            setSingleRedemption(faceAmount, redemption, redemptionDate);
        }
    }


    Real FixedRateBondOption::accrued(Date settlement) const {

        if (settlement == Date()) settlement = settlementDate();

        const bool IncludeToday = false;
        for (Size i = 0; i<cashflows_.size(); ++i) {
            // the first coupon paying after d is the one we're after
            if (!cashflows_[i]->hasOccurred(settlement, IncludeToday)) {
                ext::shared_ptr<Coupon> coupon =
                    ext::dynamic_pointer_cast<Coupon>(cashflows_[i]);
                if (coupon)
                    // !!!
                    return coupon->accruedAmount(settlement) /
                    notional(settlement) * 100.0;
                else
                    return 0.0;
            }
        }
        return 0.0;
    }


    void FixedRateBondOption::setupArguments(
        PricingEngine::arguments* args) const {

        Bond::setupArguments(args);
        BondOption::arguments* arguments =
            dynamic_cast<BondOption::arguments*>(args);

        QL_REQUIRE(arguments != 0, "no arguments given");

        Date settlement = arguments->settlementDate;

        arguments->redemption = redemption()->amount();
        arguments->redemptionDate = redemption()->date();

        const Leg& cfs = cashflows();

        arguments->couponDates.clear();
        arguments->couponDates.reserve(cfs.size() - 1);
        arguments->couponAmounts.clear();
        arguments->couponAmounts.reserve(cfs.size() - 1);

        for (Size i = 0; i<cfs.size() - 1; i++) {
            if (!cfs[i]->hasOccurred(settlement, false)) {
                arguments->couponDates.push_back(cfs[i]->date());
                arguments->couponAmounts.push_back(cfs[i]->amount());
            }
        }

        arguments->callabilityPrices.clear();
        arguments->callabilityDates.clear();
        arguments->callabilityPrices.reserve(putCallSchedule_.size());
        arguments->callabilityDates.reserve(putCallSchedule_.size());

        arguments->paymentDayCounter = paymentDayCounter_;
        arguments->frequency = frequency_;

        arguments->putCallSchedule = putCallSchedule_;
        for (Size i = 0; i<putCallSchedule_.size(); i++) {
            if (!putCallSchedule_[i]->hasOccurred(settlement, false)) {
                arguments->callabilityDates.push_back(
                    putCallSchedule_[i]->date());
                arguments->callabilityPrices.push_back(
                    putCallSchedule_[i]->price().amount());

                if (putCallSchedule_[i]->price().type() ==
                    Callability::Price::Clean) {
                    /* calling accrued() forces accrued interest to be zero
                    if future option date is also coupon date, so that dirty
                    price = clean price. Use here because callability is
                    always applied before coupon in the tree engine.
                    */
                    arguments->callabilityPrices.back() +=
                        this->accrued(putCallSchedule_[i]->date());
                }
            }
        }

        arguments->spread = 0.0;
    }
}

