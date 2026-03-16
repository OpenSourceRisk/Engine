/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/time/schedule.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/subperiodsswap.hpp>
#include <ql/currencies/america.hpp>
#include <ql/currencies/asia.hpp>
#include <ql/currencies/europe.hpp>
#include <ql/currencies/oceania.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>

using namespace QuantLib;

namespace QuantExt {

SubPeriodsSwap::SubPeriodsSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor, bool isPayer,
                               const Period& fixedTenor, Rate fixedRate, const Calendar& fixedCalendar,
                               const DayCounter& fixedDayCount, BusinessDayConvention fixedConvention,
                               const Period& floatPayTenor, const QuantLib::ext::shared_ptr<IborIndex>& iborIndex,
                               const DayCounter& floatingDayCount, DateGeneration::Rule rule,
                               QuantExt::SubPeriodsCoupon1::Type type)

    : Swap(2), nominal_(nominal), isPayer_(isPayer), fixedRate_(fixedRate), fixedDayCount_(fixedDayCount),
      floatIndex_(iborIndex), floatDayCount_(floatingDayCount), floatPayTenor_(floatPayTenor), type_(type) {

    Date terminationDate = effectiveDate + swapTenor;

    // Fixed leg
    fixedSchedule_ = MakeSchedule()
                         .from(effectiveDate)
                         .to(terminationDate)
                         .withTenor(fixedTenor)
                         .withCalendar(fixedCalendar)
                         .withConvention(fixedConvention)
                         .withTerminationDateConvention(fixedConvention)
                         .withRule(rule);

    legs_[0] = FixedRateLeg(fixedSchedule_)
                   .withNotionals(nominal_)
                   .withCouponRates(fixedRate_, fixedDayCount_)
                   .withPaymentAdjustment(fixedConvention);

    // Sub Periods Leg, schedule is the PAY schedule
    BusinessDayConvention floatPmtConvention = iborIndex->businessDayConvention();
    Calendar floatPmtCalendar = iborIndex->fixingCalendar();
    floatSchedule_ = MakeSchedule()
                         .from(effectiveDate)
                         .to(terminationDate)
                         .withTenor(floatPayTenor)
                         .withCalendar(floatPmtCalendar)
                         .withConvention(floatPmtConvention)
                         .withTerminationDateConvention(floatPmtConvention)
                         .withRule(rule);

    legs_[1] = SubPeriodsLeg1(floatSchedule_, floatIndex_)
                   .withNotional(nominal_)
                   .withPaymentAdjustment(floatPmtConvention)
                   .withPaymentDayCounter(floatDayCount_)
                   .withPaymentCalendar(floatPmtCalendar)
                   .includeSpread(false)
                   .withType(type_);

    // legs_[0] is fixed
    payer_[0] = isPayer_ ? -1.0 : +1.0;
    payer_[1] = isPayer_ ? +1.0 : -1.0;

    // Register this instrument with its coupons
    Leg::const_iterator it;
    for (it = legs_[0].begin(); it != legs_[0].end(); ++it)
        registerWith(*it);
    for (it = legs_[1].begin(); it != legs_[1].end(); ++it)
        registerWith(*it);
}

Real SubPeriodsSwap::fairRate() const {
    static const Spread basisPoint = 1.0e-4;
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return fixedRate_ - NPV_ / (legBPS_[0] / basisPoint);
}


MakeSubPeriodsSwap::MakeSubPeriodsSwap(const Period& swapTenor, const QuantLib::ext::shared_ptr<IborIndex>& index, Rate fixedRate, 
    const Period& floatPayTenor, const Period& forwardStart)
    : swapTenor_(swapTenor), index_(index), fixedRate_(fixedRate), floatPayTenor_(floatPayTenor), forwardStart_(forwardStart), 
    nominal_(1.0), isPayer_(true), settlementDays_(index->fixingDays()), fixedCalendar_(index->fixingCalendar()), 
    fixedConvention_(ModifiedFollowing), fixedRule_(DateGeneration::Backward), floatDayCounter_(index->dayCounter()),
      subCouponsType_(QuantExt::SubPeriodsCoupon1::Compounding)  {}

MakeSubPeriodsSwap::operator SubPeriodsSwap() const {
    QuantLib::ext::shared_ptr<SubPeriodsSwap> swap = *this;
    return *swap;
}

MakeSubPeriodsSwap::operator QuantLib::ext::shared_ptr<SubPeriodsSwap>() const {
    
    Date startDate;

    // start dates and end dates
    if (effectiveDate_ != Date())
        startDate = effectiveDate_;
    else {
        Date refDate = Settings::instance().evaluationDate();
        // if the evaluation date is not a business day
        // then move to the next business day
        refDate = index_->fixingCalendar().adjust(refDate);
        Date spotDate = index_->fixingCalendar().advance(refDate, settlementDays_ * Days);
        startDate = spotDate + forwardStart_;
        if (forwardStart_.length() < 0)
            startDate = index_->fixingCalendar().adjust(startDate, Preceding);
        else
            startDate = index_->fixingCalendar().adjust(startDate, Following);
    }

    // Copied from MakeVanillaSwap - Is there a better way?
    const Currency& curr = index_->currency();
    Period fixedTenor;
    if (fixedTenor_ != Period())
        fixedTenor = fixedTenor_;
    else {
        if ((curr == EURCurrency()) ||
            (curr == USDCurrency()) ||
            (curr == CHFCurrency()) ||
            (curr == SEKCurrency()) ||
            (curr == GBPCurrency() && swapTenor_ <= 1 * Years))
            fixedTenor = Period(1, Years);
        else if ((curr == GBPCurrency() && swapTenor_ > 1 * Years) ||
            (curr == JPYCurrency()) ||
            (curr == AUDCurrency() && swapTenor_ >= 4 * Years))
            fixedTenor = Period(6, Months);
        else if ((curr == HKDCurrency() ||
            (curr == AUDCurrency() && swapTenor_ < 4 * Years)))
            fixedTenor = Period(3, Months);
        else
            QL_FAIL("unknown fixed leg default tenor for " << curr);
    }

    // Copied from MakeVanillaSwap - Is there a better way?
    DayCounter fixedDayCount;
    if (fixedDayCount_ != DayCounter())
        fixedDayCount = fixedDayCount_;
    else {
        if (curr == USDCurrency())
            fixedDayCount = Actual360();
        else if (curr == EURCurrency() || curr == CHFCurrency() ||
            curr == SEKCurrency())
            fixedDayCount = Thirty360(Thirty360::BondBasis);
        else if (curr == GBPCurrency() || curr == JPYCurrency() ||
            curr == AUDCurrency() || curr == HKDCurrency() ||
            curr == THBCurrency())
            fixedDayCount = Actual365Fixed();
        else
            QL_FAIL("unknown fixed leg day counter for " << curr);
    }

    QuantLib::ext::shared_ptr<SubPeriodsSwap> swap(new SubPeriodsSwap(startDate, nominal_, swapTenor_, isPayer_, 
        fixedTenor, fixedRate_, fixedCalendar_, fixedDayCount, fixedConvention_, floatPayTenor_, index_, 
        floatDayCounter_, fixedRule_, subCouponsType_));

    if (engine_ != 0)
        swap->setPricingEngine(engine_);

    return swap;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withEffectiveDate(const Date& effectiveDate) {
    effectiveDate_ = effectiveDate;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withNominal(Real n) {
    nominal_ = n;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withIsPayer(bool p) {
    isPayer_ = p;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withSettlementDays(Natural settlementDays) {
    settlementDays_ = settlementDays;
    effectiveDate_ = Date();
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withFixedLegTenor(const Period& t) {
    fixedTenor_ = t;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withFixedLegCalendar(const Calendar& cal) {
    fixedCalendar_ = cal;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withFixedLegConvention(BusinessDayConvention bdc) {
    fixedConvention_ = bdc;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withFixedLegRule(DateGeneration::Rule r) {
    fixedRule_ = r;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withFixedLegDayCount(const DayCounter& dc) {
    fixedDayCount_ = dc;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withSubCouponsType(const QuantExt::SubPeriodsCoupon1::Type& st) {
    subCouponsType_ = st;
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withDiscountingTermStructure(const Handle<YieldTermStructure>& d) {
    bool includeSettlementDateFlows = false;
    engine_ = QuantLib::ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(d, includeSettlementDateFlows));
    return *this;
}

MakeSubPeriodsSwap& MakeSubPeriodsSwap::withPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
    return *this;
}
} // namespace QuantExt
