/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file fixedbmaswap.hpp
    \brief fixed vs averaged bma swap

    \ingroup instruments
*/

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/currencies/america.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/time/daycounters/thirty360.hpp>
#include <qle/instruments/fixedbmaswap.hpp>

using namespace QuantLib;

namespace QuantExt {

FixedBMASwap::FixedBMASwap(Type type, Real nominal,
                           // Fixed leg
                           const Schedule& fixedSchedule, Rate fixedRate, const DayCounter& fixedDayCount,
                           // BMA leg
                           const Schedule& bmaSchedule, const QuantLib::ext::shared_ptr<BMAIndex>& bmaIndex,
                           const DayCounter& bmaDayCount)
    : Swap(2), type_(type), nominal_(nominal), fixedRate_(fixedRate) {

    legs_[0] = FixedRateLeg(fixedSchedule)
                   .withNotionals(nominal)
                   .withCouponRates(fixedRate, fixedDayCount)
                   .withPaymentAdjustment(fixedSchedule.businessDayConvention());

    legs_[1] = AverageBMALeg(bmaSchedule, bmaIndex)
                   .withNotionals(nominal)
                   .withPaymentDayCounter(bmaDayCount)
                   .withPaymentAdjustment(bmaSchedule.businessDayConvention());

    for (Size j = 0; j < 2; ++j) {
        for (Leg::iterator i = legs_[j].begin(); i != legs_[j].end(); ++i)
            registerWith(*i);
    }

    switch (type_) {
    case Payer:
        payer_[0] = +1.0;
        payer_[1] = -1.0;
        break;
    case Receiver:
        payer_[0] = -1.0;
        payer_[1] = +1.0;
        break;
    default:
        QL_FAIL("Unknown BMA-swap type");
    }
}

Real FixedBMASwap::fixedRate() const { return fixedRate_; }

Real FixedBMASwap::nominal() const { return nominal_; }

FixedBMASwap::Type FixedBMASwap::type() const { return type_; }

const Leg& FixedBMASwap::fixedLeg() const { return legs_[0]; }

const Leg& FixedBMASwap::bmaLeg() const { return legs_[1]; }

Real FixedBMASwap::fixedLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[0] != Null<Real>(), "result not available");
    return legBPS_[0];
}

Real FixedBMASwap::fixedLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[0] != Null<Real>(), "result not available");
    return legNPV_[0];
}

Rate FixedBMASwap::fairRate() const {
    calculate();
    QL_REQUIRE(fairRate_ != Null<Rate>(), "result not available");
    return fairRate_;
}

Real FixedBMASwap::bmaLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[1] != Null<Real>(), "result not available");
    return legBPS_[1];
}

Real FixedBMASwap::bmaLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[1] != Null<Real>(), "result not available");
    return legNPV_[1];
}

void FixedBMASwap::fetchResults(const PricingEngine::results* r) const {
    static const Spread basisPoint = 1.0e-4;

    Swap::fetchResults(r);

    const FixedBMASwap::results* results = dynamic_cast<const FixedBMASwap::results*>(r);
    if (results) { // might be a swap engine, so no error is thrown
        fairRate_ = results->fairRate;
    } else {
        fairRate_ = Null<Rate>();
    }
    if (fairRate_ == Null<Rate>()) {
        // calculate it from other results
        if (legBPS_[0] != Null<Real>())
            fairRate_ = fixedRate_ - NPV_ / (legBPS_[0] / basisPoint);
    }
}

void FixedBMASwap::results::reset() {
    Swap::results::reset();
    fairRate = Null<Rate>();
}

MakeFixedBMASwap::MakeFixedBMASwap(const Period& swapTenor, const QuantLib::ext::shared_ptr<BMAIndex>& index, Rate fixedRate,
                                   const Period& forwardStart)
    : swapTenor_(swapTenor), bmaIndex_(index), fixedRate_(fixedRate), forwardStart_(forwardStart),
      settlementDays_(bmaIndex_->fixingDays()), fixedCalendar_(index->fixingCalendar()),
      bmaCalendar_(index->fixingCalendar()), type_(FixedBMASwap::Payer), nominal_(1.0), bmaLegTenor_(3 * Months),
      fixedConvention_(ModifiedFollowing), fixedTerminationDateConvention_(ModifiedFollowing),
      bmaConvention_(ModifiedFollowing), bmaTerminationDateConvention_(ModifiedFollowing),
      fixedRule_(DateGeneration::Backward), bmaRule_(DateGeneration::Backward), fixedEndOfMonth_(false),
      bmaEndOfMonth_(false), fixedFirstDate_(Date()), fixedNextToLastDate_(Date()), bmaFirstDate_(Date()),
      bmaNextToLastDate_(Date()), bmaDayCount_(index->dayCounter()) {}

MakeFixedBMASwap::operator FixedBMASwap() const {
    QuantLib::ext::shared_ptr<FixedBMASwap> swap = *this;
    return *swap;
}

MakeFixedBMASwap::operator QuantLib::ext::shared_ptr<FixedBMASwap>() const {
    Date startDate;

    // start dates and end dates
    if (effectiveDate_ != Date())
        startDate = effectiveDate_;
    else {
        Date refDate = Settings::instance().evaluationDate();
        // if the evaluation date is not a business day
        // then move to the next business day
        refDate = bmaCalendar_.adjust(refDate);
        Date spotDate = bmaCalendar_.advance(refDate, settlementDays_ * Days);
        startDate = spotDate + forwardStart_;
        if (forwardStart_.length() < 0)
            startDate = bmaCalendar_.adjust(startDate, Preceding);
        else
            startDate = bmaCalendar_.adjust(startDate, Following);
    }

    Date endDate = terminationDate_;
    if (endDate == Date()) {
        if (bmaEndOfMonth_)
            endDate = bmaCalendar_.advance(startDate, swapTenor_, ModifiedFollowing, bmaEndOfMonth_);
        else
            endDate = startDate + swapTenor_;
    }

    const Currency& curr = bmaIndex_->currency();
    QL_REQUIRE(curr == USDCurrency(), "Only USD is supported for fixed vs BMA swaps.");

    // schedules

    Period fixedTenor;
    if (fixedTenor_ != Period())
        fixedTenor = fixedTenor_;
    else
        // Default according to Bloomberg & OpenGamma
        fixedTenor = 6 * Months;

    Schedule fixedSchedule(startDate, endDate, fixedTenor, fixedCalendar_, fixedConvention_,
                           fixedTerminationDateConvention_, fixedRule_, fixedEndOfMonth_, fixedFirstDate_,
                           fixedNextToLastDate_);

    Schedule bmaSchedule(startDate, endDate, bmaLegTenor_, bmaCalendar_, bmaConvention_, bmaTerminationDateConvention_,
                         bmaRule_, bmaEndOfMonth_, bmaFirstDate_, bmaNextToLastDate_);

    DayCounter fixedDayCount;
    if (fixedDayCount_ != DayCounter())
        fixedDayCount = fixedDayCount_;
    else {
        // Default according to Bloomberg & OpenGamma
        fixedDayCount = Thirty360(Thirty360::USA);
    }

    Rate usedFixedRate = fixedRate_;
    if (fixedRate_ == Null<Rate>()) {
        FixedBMASwap temp(type_, nominal_, fixedSchedule,
                          0.0, // fixed rate
                          fixedDayCount, bmaSchedule, bmaIndex_, bmaDayCount_);
        if (engine_ != 0)
            temp.setPricingEngine(engine_);
        else
            QL_FAIL("Null fixed rate and no discounting curve provided to fixed vs BMA swap.");
        usedFixedRate = temp.fairRate();
    }

    QuantLib::ext::shared_ptr<FixedBMASwap> swap(new FixedBMASwap(type_, nominal_, fixedSchedule, usedFixedRate, fixedDayCount,
                                                          bmaSchedule, bmaIndex_, bmaDayCount_));

    if (engine_ != 0)
        swap->setPricingEngine(engine_);

    return swap;
}

MakeFixedBMASwap& MakeFixedBMASwap::receiveFixed(bool flag) {
    type_ = flag ? FixedBMASwap::Receiver : FixedBMASwap::Payer;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withType(FixedBMASwap::Type type) {
    type_ = type;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withNominal(Real n) {
    nominal_ = n;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withBMALegTenor(const Period& tenor) {
    QL_REQUIRE(tenor.units() == Months, "Average BMA Leg coupons should pay as a multiple of months.");
    bmaLegTenor_ = tenor;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withSettlementDays(Natural settlementDays) {
    settlementDays_ = settlementDays;
    effectiveDate_ = Date();
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withEffectiveDate(const Date& effectiveDate) {
    effectiveDate_ = effectiveDate;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withTerminationDate(const Date& terminationDate) {
    terminationDate_ = terminationDate;
    swapTenor_ = Period();
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withDiscountingTermStructure(const Handle<YieldTermStructure>& d) {
    bool includeSettlementDateFlows = false;
    engine_ = QuantLib::ext::shared_ptr<PricingEngine>(new DiscountingSwapEngine(d, includeSettlementDateFlows));
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withPricingEngine(const QuantLib::ext::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegTenor(const Period& t) {
    fixedTenor_ = t;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegCalendar(const Calendar& cal) {
    fixedCalendar_ = cal;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegConvention(BusinessDayConvention bdc) {
    fixedConvention_ = bdc;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegTerminationDateConvention(BusinessDayConvention bdc) {
    fixedTerminationDateConvention_ = bdc;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegRule(DateGeneration::Rule r) {
    fixedRule_ = r;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegEndOfMonth(bool flag) {
    fixedEndOfMonth_ = flag;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegFirstDate(const Date& d) {
    fixedFirstDate_ = d;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegNextToLastDate(const Date& d) {
    fixedNextToLastDate_ = d;
    return *this;
}

MakeFixedBMASwap& MakeFixedBMASwap::withFixedLegDayCount(const DayCounter& dc) {
    fixedDayCount_ = dc;
    return *this;
}
} // namespace QuantExt
