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

#include <qle/instruments/crossccyfixfloatmtmresetswap.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>

#include <boost/make_shared.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>

using namespace QuantLib;

namespace QuantExt {

CrossCcyFixFloatMtMResetSwap::CrossCcyFixFloatMtMResetSwap(
    Real nominal, const Currency& fixedCurrency, const Schedule& fixedSchedule,
    Rate fixedRate, const DayCounter& fixedDayCount, const BusinessDayConvention& fixedPaymentBdc,
    Natural fixedPaymentLag, const Calendar& fixedPaymentCalendar, const Currency& floatCurrency,
    const Schedule& floatSchedule, const QuantLib::ext::shared_ptr<IborIndex>& floatIndex, Spread floatSpread,
    const BusinessDayConvention& floatPaymentBdc, Natural floatPaymentLag, const Calendar& floatPaymentCalendar,
    const QuantLib::ext::shared_ptr<FxIndex>& fxIdx, bool resetsOnFloatLeg, bool receiveFixed)
    : CrossCcySwap(3), nominal_(nominal), fixedCurrency_(fixedCurrency),
    fixedSchedule_(fixedSchedule), fixedRate_(fixedRate), fixedDayCount_(fixedDayCount), 
    fixedPaymentBdc_(fixedPaymentBdc), fixedPaymentLag_(fixedPaymentLag), fixedPaymentCalendar_(fixedPaymentCalendar),
    floatCurrency_(floatCurrency), floatSchedule_(floatSchedule), floatIndex_(floatIndex),
    floatSpread_(floatSpread), floatPaymentBdc_(floatPaymentBdc),
    floatPaymentLag_(floatPaymentLag), floatPaymentCalendar_(floatPaymentCalendar),
    fxIndex_(fxIdx), resetsOnFloatLeg_(resetsOnFloatLeg), receiveFixed_(receiveFixed) {

    registerWith(floatIndex_);
    registerWith(fxIndex_);
    initialize();
}

void CrossCcyFixFloatMtMResetSwap::initialize() {

    // if resets on floating leg, set notional to zero, else the fixed
    Real floatNotional, fixedNotional;
    if (resetsOnFloatLeg_) {
        floatNotional = 0.0;
        fixedNotional = nominal_;
    } else {
        floatNotional = nominal_;
        fixedNotional = 0.0;
    }

    // Build the float leg
    Leg floatLeg = IborLeg(floatSchedule_, floatIndex_)
        .withNotionals(floatNotional)
        .withSpreads(floatSpread_)
        .withPaymentAdjustment(floatPaymentBdc_)
        .withPaymentLag(floatPaymentLag_)
        .withPaymentCalendar(floatPaymentCalendar_);

    // Register with each floating rate coupon
    for (Leg::const_iterator it = floatLeg.begin(); it < floatLeg.end(); ++it)
        registerWith(*it);

    // Build the fixed rate leg
    Leg fixedLeg = FixedRateLeg(fixedSchedule_)
        .withNotionals(fixedNotional)
        .withCouponRates(fixedRate_, fixedDayCount_)
        .withPaymentAdjustment(fixedPaymentBdc_)
        .withPaymentLag(fixedPaymentLag_)
        .withPaymentCalendar(fixedPaymentCalendar_);

    if (resetsOnFloatLeg_) {
        // Notional exchanges for fixed leg
        // Initial notional exchange
        Date aDate = fixedSchedule_.dates().front();
        aDate = fixedPaymentCalendar_.adjust(aDate, fixedPaymentBdc_);
        QuantLib::ext::shared_ptr<CashFlow> aCashflow = QuantLib::ext::make_shared<SimpleCashFlow>(-fixedNotional, aDate);
        fixedLeg.insert(fixedLeg.begin(), aCashflow);

        // Final notional exchange
        aDate = fixedLeg.back()->date();
        aCashflow = QuantLib::ext::make_shared<SimpleCashFlow>(fixedNotional, aDate);
        fixedLeg.push_back(aCashflow);

        // resetting floating leg
        for (Size j = 0; j < floatLeg.size(); ++j) {
            QuantLib::ext::shared_ptr<FloatingRateCoupon> coupon = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(floatLeg[j]);
            Date fixingDate = fxIndex_->fixingCalendar().advance(coupon->accrualStartDate(),
                -static_cast<Integer>(fxIndex_->fixingDays()), Days);
            QuantLib::ext::shared_ptr<FloatingRateFXLinkedNotionalCoupon> fxLinkedCoupon =
                QuantLib::ext::make_shared<FloatingRateFXLinkedNotionalCoupon>(fixingDate, fixedNotional, fxIndex_, coupon);
            floatLeg[j] = fxLinkedCoupon;
        }

        // now build a separate leg to store the resetting notionals
        receiveFixed_ ? payer_[2] = -1.0 : payer_[2] = +1.0;
        currencies_[2] = floatCurrency_;
        for (Size j = 0; j < floatLeg.size(); j++) {
            QuantLib::ext::shared_ptr<Coupon> c = QuantLib::ext::dynamic_pointer_cast<Coupon>(floatLeg[j]);
            QL_REQUIRE(c, "Resetting XCCY - expected Coupon"); 
            // build a pair of notional flows, one at the start and one at the end of
            // the accrual period. Both with the same FX fixing date
            Date fixingDate = fxIndex_->fixingCalendar().advance(c->accrualStartDate(),
                -static_cast<Integer>(fxIndex_->fixingDays()), Days);
            legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
                new FXLinkedCashFlow(c->accrualStartDate(), fixingDate, -fixedNotional, fxIndex_)));
            legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
                new FXLinkedCashFlow(c->accrualEndDate(), fixingDate, fixedNotional, fxIndex_)));
        }
    } else {
        // Notional exchanges for floating leg
        // Initial notional exchange
        Date aDate = floatSchedule_.dates().front();
        aDate = floatPaymentCalendar_.adjust(aDate, floatPaymentBdc_);
        QuantLib::ext::shared_ptr<CashFlow> aCashflow = QuantLib::ext::make_shared<SimpleCashFlow>(-floatNotional, aDate);
        floatLeg.insert(floatLeg.begin(), aCashflow);

        // Final notional exchange
        aDate = fixedLeg.back()->date();
        aCashflow = QuantLib::ext::make_shared<SimpleCashFlow>(floatNotional, aDate);
        floatLeg.push_back(aCashflow);

        // resetting fixed leg
        for (Size j = 0; j < fixedLeg.size(); ++j) {
            QuantLib::ext::shared_ptr<FixedRateCoupon> coupon = QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(fixedLeg[j]);
            Date fixingDate = fxIndex_->fixingCalendar().advance(coupon->accrualStartDate(),
                -static_cast<Integer>(fxIndex_->fixingDays()), Days);
            QuantLib::ext::shared_ptr<FixedRateFXLinkedNotionalCoupon> fxLinkedCoupon = 
                QuantLib::ext::make_shared<FixedRateFXLinkedNotionalCoupon>(fixingDate, floatNotional, fxIndex_, coupon);
            floatLeg[j] = fxLinkedCoupon;
        }

        // now build a separate leg to store the resetting notionals
        receiveFixed_ ? payer_[2] = -1.0 : payer_[2] = +1.0;
        currencies_[2] = fixedCurrency_;
        for (Size j = 0; j < fixedLeg.size(); j++) {
            QuantLib::ext::shared_ptr<Coupon> c = QuantLib::ext::dynamic_pointer_cast<Coupon>(fixedLeg[j]);
            QL_REQUIRE(c, "Resetting XCCY - expected Coupon");
            // build a pair of notional flows, one at the start and one at the end of
            // the accrual period. Both with the same FX fixing date
            Date fixingDate = fxIndex_->fixingCalendar().advance(c->accrualStartDate(),
                -static_cast<Integer>(fxIndex_->fixingDays()), Days);
            legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
                new FXLinkedCashFlow(c->accrualStartDate(), fixingDate, -floatNotional, fxIndex_)));
            legs_[2].push_back(QuantLib::ext::shared_ptr<CashFlow>(
                new FXLinkedCashFlow(c->accrualEndDate(), fixingDate, floatNotional, fxIndex_)));
        }
    }

    // Deriving from cross currency swap where:
    //   First leg should hold the pay flows
    //   Second leg should hold the receive flows
    payer_[0] = -1.0;
    payer_[1] = 1.0;
    if (receiveFixed_) {
        legs_[1] = fixedLeg;
        currencies_[1] = fixedCurrency_;
        legs_[0] = floatLeg;
        currencies_[0] = floatCurrency_;
    } else {
        legs_[0] = fixedLeg;
        currencies_[0] = fixedCurrency_;
        legs_[1] = floatLeg;
        currencies_[1] = floatCurrency_;
    }
    
    // Register the instrument with all cashflows on each leg.
    for (Size legNo = 0; legNo < legs_.size(); legNo++) {
        Leg::iterator it;
        for (it = legs_[legNo].begin(); it != legs_[legNo].end(); ++it) {
            registerWith(*it);
        }
    }
}

void CrossCcyFixFloatMtMResetSwap::setupArguments(PricingEngine::arguments* a) const {

    CrossCcySwap::setupArguments(a);

    if (CrossCcyFixFloatMtMResetSwap::arguments* args = dynamic_cast<CrossCcyFixFloatMtMResetSwap::arguments*>(a)) {
        args->fixedRate = fixedRate_;
        args->spread = floatSpread_;
    }
}

void CrossCcyFixFloatMtMResetSwap::fetchResults(const PricingEngine::results* r) const {

    CrossCcySwap::fetchResults(r);

    // Depending on the pricing engine used, we may have CrossCcyFixFloatSwap::results
    if (const CrossCcyFixFloatMtMResetSwap::results* res = dynamic_cast<const CrossCcyFixFloatMtMResetSwap::results*>(r)) {
        // If we have CrossCcyFixFloatSwap::results from the pricing engine
        fairFixedRate_ = res->fairFixedRate;
        fairSpread_ = res->fairSpread;
    } else {
        // If not, set them to Null to indicate a calculation is needed below
        fairFixedRate_ = Null<Rate>();
        fairSpread_ = Null<Spread>();
    }

    // Calculate fair rate and spread if they are still Null here
    static Spread basisPoint = 1.0e-4;

    Size idxFixed = receiveFixed_ ? 1 : 0;
    if (fairFixedRate_ == Null<Rate>() && legBPS_[idxFixed] != Null<Real>())
        fairFixedRate_ = fixedRate_ - NPV_ / (legBPS_[idxFixed] / basisPoint);

    Size idxFloat = receiveFixed_ ? 0 : 1;
    if (fairSpread_ == Null<Spread>() && legBPS_[idxFloat] != Null<Real>())
        fairSpread_ = floatSpread_ - NPV_ / (legBPS_[idxFloat] / basisPoint);
}

void CrossCcyFixFloatMtMResetSwap::setupExpired() const {
    CrossCcySwap::setupExpired();
    fairFixedRate_ = Null<Rate>();
    fairSpread_ = Null<Spread>();
}

void CrossCcyFixFloatMtMResetSwap::arguments::validate() const {
    CrossCcySwap::arguments::validate();
    QL_REQUIRE(fixedRate != Null<Rate>(), "Fixed rate cannot be null");
    QL_REQUIRE(spread != Null<Spread>(), "Spread cannot be null");
}

void CrossCcyFixFloatMtMResetSwap::results::reset() {
    CrossCcySwap::results::reset();
    fairFixedRate = Null<Rate>();
    fairSpread = Null<Spread>();
}
} // namespace QuantExt
