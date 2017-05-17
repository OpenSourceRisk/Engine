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

#include <ql/cashflows/iborcoupon.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/pricingengines/swap/discountingswapengine.hpp>

#include <qle/instruments/tenorbasisswap.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {
class FairShortSpreadHelper {
public:
    FairShortSpreadHelper(const TenorBasisSwap& swap, const Handle<YieldTermStructure>& discountCurve, Real longLegNPV)
        : longLegNPV_(longLegNPV) {

        engine_ = boost::shared_ptr<PricingEngine>(new DiscountingSwapEngine(discountCurve));
        arguments_ = dynamic_cast<Swap::arguments*>(engine_->getArguments());
        swap.setupArguments(arguments_);
        shortNo_ = swap.payLongIndex();
        shortLeg_ = arguments_->legs[shortNo_];
        results_ = dynamic_cast<const Swap::results*>(engine_->getResults());
    }

    Real operator()(Spread shortSpread) const {
        // Change the spread on the leg and recalculate
        Leg::const_iterator it;
        for (it = shortLeg_.begin(); it != shortLeg_.end(); ++it) {
            boost::shared_ptr<SubPeriodsCoupon> c = boost::dynamic_pointer_cast<SubPeriodsCoupon>(*it);
            c->spread() = shortSpread;
        }
        engine_->calculate();
        return results_->legNPV[shortNo_] + longLegNPV_;
    }

private:
    boost::shared_ptr<PricingEngine> engine_;
    Real longLegNPV_;
    const Swap::results* results_;
    Swap::arguments* arguments_;
    Size shortNo_;
    Leg shortLeg_;
};
} // namespace

TenorBasisSwap::TenorBasisSwap(const Date& effectiveDate, Real nominal, const Period& swapTenor, bool payLongIndex,
                               const boost::shared_ptr<IborIndex>& longIndex, Spread longSpread,
                               const boost::shared_ptr<IborIndex>& shortIndex, Spread shortSpread,
                               const Period& shortPayTenor, DateGeneration::Rule rule, bool includeSpread,
                               SubPeriodsCoupon::Type type)
    : Swap(2), nominal_(nominal), payLongIndex_(payLongIndex), longIndex_(longIndex), longSpread_(longSpread),
      shortIndex_(shortIndex), shortSpread_(shortSpread), shortPayTenor_(shortPayTenor), includeSpread_(includeSpread),
      type_(type) {

    // Checks
    Period longTenor = longIndex_->tenor();
    QL_REQUIRE(shortPayTenor_ >= shortIndex_->tenor(), "Expected short payment tenor to exceed/equal shortIndex tenor");
    QL_REQUIRE(shortPayTenor_ <= longTenor, "Expected short payment tenor to be at most longSchedule tenor");

    // Create the default long and short schedules
    Date terminationDate = effectiveDate + swapTenor;

    longSchedule_ = MakeSchedule()
                        .from(effectiveDate)
                        .to(terminationDate)
                        .withTenor(longTenor)
                        .withCalendar(longIndex_->fixingCalendar())
                        .withConvention(longIndex_->businessDayConvention())
                        .withTerminationDateConvention(longIndex_->businessDayConvention())
                        .withRule(rule)
                        .endOfMonth(longIndex_->endOfMonth());

    if (shortPayTenor_ == shortIndex_->tenor()) {
        shortSchedule_ = MakeSchedule()
                             .from(effectiveDate)
                             .to(terminationDate)
                             .withTenor(shortPayTenor_)
                             .withCalendar(shortIndex_->fixingCalendar())
                             .withConvention(shortIndex_->businessDayConvention())
                             .withTerminationDateConvention(shortIndex_->businessDayConvention())
                             .withRule(rule)
                             .endOfMonth(shortIndex_->endOfMonth());
    } else {
        /* Where the payment tenor is longer, the SubPeriodsLeg
           will handle the adjustments. We just need to give the
           anchor dates. */
        shortSchedule_ = MakeSchedule()
                             .from(effectiveDate)
                             .to(terminationDate)
                             .withTenor(shortPayTenor_)
                             .withCalendar(NullCalendar())
                             .withConvention(QuantLib::Unadjusted)
                             .withTerminationDateConvention(QuantLib::Unadjusted)
                             .withRule(rule)
                             .endOfMonth(shortIndex_->endOfMonth());
    }

    // Create legs
    initializeLegs();
}

TenorBasisSwap::TenorBasisSwap(Real nominal, bool payLongIndex, const Schedule& longSchedule,
                               const boost::shared_ptr<IborIndex>& longIndex, Spread longSpread,
                               const Schedule& shortSchedule, const boost::shared_ptr<IborIndex>& shortIndex,
                               Spread shortSpread, bool includeSpread, SubPeriodsCoupon::Type type)
    : Swap(2), nominal_(nominal), payLongIndex_(payLongIndex), longSchedule_(longSchedule), longIndex_(longIndex),
      longSpread_(longSpread), shortSchedule_(shortSchedule), shortIndex_(shortIndex), shortSpread_(shortSpread),
      includeSpread_(includeSpread), type_(type) {

    // Checks
    Period longTenor = longSchedule_.tenor();
    QL_REQUIRE(longTenor == longIndex_->tenor(), "Expected longSchedule tenor to equal longIndex tenor");
    shortPayTenor_ = shortSchedule_.tenor();
    QL_REQUIRE(shortPayTenor_ >= shortIndex_->tenor(), "Expected shortSchedule tenor to exceed/equal shortIndex tenor");
    QL_REQUIRE(shortPayTenor_ <= longTenor, "Expected shortSchedule tenor to be at most longSchedule tenor");

    // Create legs
    initializeLegs();
}

void TenorBasisSwap::initializeLegs() {

    // Long leg
    BusinessDayConvention longPmtConvention = longIndex_->businessDayConvention();
    DayCounter longDayCounter = longIndex_->dayCounter();
    Leg longLeg = IborLeg(longSchedule_, longIndex_)
                      .withNotionals(nominal_)
                      .withSpreads(longSpread_)
                      .withPaymentAdjustment(longPmtConvention)
                      .withPaymentDayCounter(longDayCounter);

    // Short leg
    Leg shortLeg;
    BusinessDayConvention shortPmtConvention = shortIndex_->businessDayConvention();
    DayCounter shortDayCounter = shortIndex_->dayCounter();
    Calendar shortPmtCalendar = shortIndex_->fixingCalendar();
    if (shortPayTenor_ == shortIndex_->tenor()) {
        shortLeg = IborLeg(shortSchedule_, shortIndex_)
                       .withNotionals(nominal_)
                       .withSpreads(shortSpread_)
                       .withPaymentAdjustment(shortPmtConvention)
                       .withPaymentDayCounter(shortDayCounter);
    } else {
        shortLeg = SubPeriodsLeg(shortSchedule_, shortIndex_)
                       .withNotional(nominal_)
                       .withSpread(shortSpread_)
                       .withPaymentAdjustment(shortPmtConvention)
                       .withPaymentDayCounter(shortDayCounter)
                       .withPaymentCalendar(shortPmtCalendar)
                       .includeSpread(includeSpread_)
                       .withType(type_);
    }

    // Pay (Rec) leg is legs_[0] (legs_[1])
    payer_[0] = -1.0;
    payer_[1] = +1.0;

    longNo_ = !payLongIndex_;
    shortNo_ = payLongIndex_;
    legs_[longNo_] = longLeg;
    legs_[shortNo_] = shortLeg;

    // Register this instrument with its coupons
    Leg::const_iterator it;
    for (it = legs_[0].begin(); it != legs_[0].end(); ++it)
        registerWith(*it);
    for (it = legs_[1].begin(); it != legs_[1].end(); ++it)
        registerWith(*it);
}

Real TenorBasisSwap::longLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[longNo_] != Null<Real>(), "Long leg BPS not available");
    return legBPS_[longNo_];
}

Real TenorBasisSwap::longLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[longNo_] != Null<Real>(), "Long leg NPV not available");
    return legNPV_[longNo_];
}

Rate TenorBasisSwap::fairLongLegSpread() const {
    calculate();
    QL_REQUIRE(fairLongSpread_ != Null<Spread>(), "Long leg fair spread not available");
    return fairLongSpread_;
}

Real TenorBasisSwap::shortLegBPS() const {
    calculate();
    QL_REQUIRE(legBPS_[shortNo_] != Null<Real>(), "Short leg BPS not available");
    return legBPS_[shortNo_];
}

Real TenorBasisSwap::shortLegNPV() const {
    calculate();
    QL_REQUIRE(legNPV_[shortNo_] != Null<Real>(), "Short leg NPV not available");
    return legNPV_[shortNo_];
}

Spread TenorBasisSwap::fairShortLegSpread() const {
    calculate();
    QL_REQUIRE(fairShortSpread_ != Null<Spread>(), "Short leg fair spread not available");
    return fairShortSpread_;
}

void TenorBasisSwap::setupExpired() const {
    Swap::setupExpired();
    fairLongSpread_ = Null<Spread>();
    fairShortSpread_ = Null<Spread>();
}

void TenorBasisSwap::fetchResults(const PricingEngine::results* r) const {

    static const Spread basisPoint = 1.0e-4;
    Swap::fetchResults(r);

    const TenorBasisSwap::results* results = dynamic_cast<const TenorBasisSwap::results*>(r);

    if (results) {
        fairLongSpread_ = results->fairLongSpread;
        fairShortSpread_ = results->fairShortSpread;
    } else {
        fairLongSpread_ = Null<Spread>();
        fairShortSpread_ = Null<Spread>();
    }

    // Long fair spread should be fine - no averaging or compounding
    if (fairLongSpread_ == Null<Spread>()) {
        if (legBPS_[longNo_] != Null<Real>()) {
            fairLongSpread_ = longSpread_ - NPV_ / (legBPS_[longNo_] / basisPoint);
        }
    }

    /* Short fair spread calculation ok if no averaging/compounding OR
       if there is averaging/compounding and the spread is added after */
    if (fairShortSpread_ == Null<Spread>()) {
        if (shortPayTenor_ == shortIndex_->tenor() || !includeSpread_) {
            if (legBPS_[shortNo_] != Null<Real>()) {
                fairShortSpread_ = shortSpread_ - NPV_ / (legBPS_[shortNo_] / basisPoint);
            }
        } else {
            // Need the discount curve
            Handle<YieldTermStructure> discountCurve;
            boost::shared_ptr<DiscountingSwapEngine> engine =
                boost::dynamic_pointer_cast<DiscountingSwapEngine>(engine_);
            if (engine) {
                discountCurve = engine->discountCurve();
                // Calculate a guess
                Spread guess = 0.0;
                if (legBPS_[shortNo_] != Null<Real>()) {
                    guess = shortSpread_ - NPV_ / (legBPS_[shortNo_] / basisPoint);
                }
                // Attempt to solve for fair spread
                Spread step = 1e-4;
                Real accuracy = 1e-8;
                FairShortSpreadHelper f(*this, discountCurve, legNPV_[longNo_]);
                Brent solver;
                fairShortSpread_ = solver.solve(f, accuracy, guess, step);
            }
        }
    }
}

void TenorBasisSwap::results::reset() {
    Swap::results::reset();
    fairLongSpread = Null<Spread>();
    fairShortSpread = Null<Spread>();
}
} // namespace QuantExt
