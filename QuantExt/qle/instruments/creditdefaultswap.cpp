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

/*
 Copyright (C) 2008, 2009 Jose Aparicio
 Copyright (C) 2008 Roland Lichters
 Copyright (C) 2008 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <qle/instruments/creditdefaultswap.hpp>
#include <qle/pricingengines/midpointcdsengine.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/credit/flathazardrate.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

namespace QuantExt {

CreditDefaultSwap::CreditDefaultSwap(Protection::Side side, Real notional, Rate spread, const Schedule& schedule,
                                     BusinessDayConvention convention, const DayCounter& dayCounter,
                                     bool settlesAccrual, bool paysAtDefaultTime, const Date& protectionStart,
                                     const boost::shared_ptr<Claim>& claim)
    : side_(side), notional_(notional), upfront_(boost::none), runningSpread_(spread), settlesAccrual_(settlesAccrual),
      paysAtDefaultTime_(paysAtDefaultTime), claim_(claim),
      protectionStart_(protectionStart == Null<Date>() ? schedule[0] : protectionStart) {
    QL_REQUIRE((schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) ||
                   protectionStart_ <= schedule[0],
               "protection can not start after accrual for (pre big bang-) CDS");
    leg_ = FixedRateLeg(schedule)
               .withNotionals(notional)
               .withCouponRates(spread, dayCounter)
               .withPaymentAdjustment(convention);

    // acrual rebate
    if (schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) {
        boost::shared_ptr<FixedRateCoupon> firstCoupon = boost::dynamic_pointer_cast<FixedRateCoupon>(leg_[0]);
        // adjust to T+3 standard settlement, assuming that protection start
        // is set to T+1 for standard CDS
        Date rebateDate =
            schedule.calendar().advance(schedule.calendar().adjust(protectionStart_, convention), 2, Days, convention);
        accrualRebate_.reset(new SimpleCashFlow(firstCoupon->accruedAmount(protectionStart_), rebateDate));
    }

    if (!claim_)
        claim_ = boost::shared_ptr<Claim>(new FaceValueClaim);
    registerWith(claim_);

    // the upfront payment left intentionally unitialized to indicate
    // this CDS has none.

    maturity_ = schedule.dates().back();
}

CreditDefaultSwap::CreditDefaultSwap(Protection::Side side, Real notional, Rate upfront, Rate runningSpread,
                                     const Schedule& schedule, BusinessDayConvention convention,
                                     const DayCounter& dayCounter, bool settlesAccrual, bool paysAtDefaultTime,
                                     const Date& protectionStart, const Date& upfrontDate,
                                     const boost::shared_ptr<Claim>& claim)
    : side_(side), notional_(notional), upfront_(upfront), runningSpread_(runningSpread),
      settlesAccrual_(settlesAccrual), paysAtDefaultTime_(paysAtDefaultTime), claim_(claim),
      protectionStart_(protectionStart == Null<Date>() ? schedule[0] : protectionStart) {

    QL_REQUIRE((schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) ||
                   protectionStart_ <= schedule[0],
               "protection can not start after accrual for (pre big bang-) CDS");

    leg_ = FixedRateLeg(schedule)
               .withNotionals(notional)
               .withCouponRates(runningSpread, dayCounter)
               .withPaymentAdjustment(convention);

    // If empty, adjust to T+3 standard settlement
    Date effectiveUpfrontDate =
        upfrontDate == Null<Date>()
            ? schedule.calendar().advance(schedule.calendar().adjust(protectionStart_, convention), 2, Days, convention)
            : upfrontDate;
    // '2' is used above since the protection start is assumed to be
    // on trade_date + 1
    upfrontPayment_.reset(new SimpleCashFlow(notional * upfront, effectiveUpfrontDate));
    QL_REQUIRE(upfrontPayment_->date() >= protectionStart_, "upfront can not be due before contract start");

    if (schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) {
        boost::shared_ptr<FixedRateCoupon> firstCoupon = boost::dynamic_pointer_cast<FixedRateCoupon>(leg_[0]);
        // adjust to T+3 standard settlement
        const Date& rebateDate = effectiveUpfrontDate;
        accrualRebate_.reset(new SimpleCashFlow(firstCoupon->accruedAmount(protectionStart_), rebateDate));
    }

    if (!claim_)
        claim_ = boost::shared_ptr<Claim>(new FaceValueClaim);
    registerWith(claim_);

    maturity_ = schedule.dates().back();
}

Protection::Side CreditDefaultSwap::side() const { return side_; }

Real CreditDefaultSwap::notional() const { return notional_; }

Rate CreditDefaultSwap::runningSpread() const { return runningSpread_; }

boost::optional<Rate> CreditDefaultSwap::upfront() const { return upfront_; }

bool CreditDefaultSwap::settlesAccrual() const { return settlesAccrual_; }

bool CreditDefaultSwap::paysAtDefaultTime() const { return paysAtDefaultTime_; }

const Leg& CreditDefaultSwap::coupons() const { return leg_; }

bool CreditDefaultSwap::isExpired() const {
    for (Leg::const_reverse_iterator i = leg_.rbegin(); i != leg_.rend(); ++i) {
        if (!(*i)->hasOccurred())
            return false;
    }
    return true;
}

void CreditDefaultSwap::setupExpired() const {
    Instrument::setupExpired();
    fairSpread_ = fairUpfront_ = 0.0;
    couponLegBPS_ = upfrontBPS_ = 0.0;
    couponLegNPV_ = defaultLegNPV_ = upfrontNPV_ = 0.0;
}

void CreditDefaultSwap::setupArguments(PricingEngine::arguments* args) const {
    CreditDefaultSwap::arguments* arguments = dynamic_cast<CreditDefaultSwap::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");

    arguments->side = side_;
    arguments->notional = notional_;
    arguments->leg = leg_;
    arguments->upfrontPayment = upfrontPayment_;
    arguments->accrualRebate = accrualRebate_;
    arguments->settlesAccrual = settlesAccrual_;
    arguments->paysAtDefaultTime = paysAtDefaultTime_;
    arguments->claim = claim_;
    arguments->upfront = upfront_;
    arguments->spread = runningSpread_;
    arguments->protectionStart = protectionStart_;
    arguments->maturity = maturity_;
}

void CreditDefaultSwap::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);

    const CreditDefaultSwap::results* results = dynamic_cast<const CreditDefaultSwap::results*>(r);
    QL_REQUIRE(results != 0, "wrong result type");

    fairSpread_ = results->fairSpread;
    fairUpfront_ = results->fairUpfront;
    couponLegBPS_ = results->couponLegBPS;
    couponLegNPV_ = results->couponLegNPV;
    defaultLegNPV_ = results->defaultLegNPV;
    upfrontNPV_ = results->upfrontNPV;
    upfrontBPS_ = results->upfrontBPS;
    accrualRebateNPV_ = results->accrualRebateNPV;
}

Rate CreditDefaultSwap::fairUpfront() const {
    calculate();
    QL_REQUIRE(fairUpfront_ != Null<Rate>(), "fair upfront not available");
    return fairUpfront_;
}

Rate CreditDefaultSwap::fairSpread() const {
    calculate();
    QL_REQUIRE(fairSpread_ != Null<Rate>(), "fair spread not available");
    return fairSpread_;
}

Real CreditDefaultSwap::couponLegBPS() const {
    calculate();
    QL_REQUIRE(couponLegBPS_ != Null<Rate>(), "coupon-leg BPS not available");
    return couponLegBPS_;
}

Real CreditDefaultSwap::couponLegNPV() const {
    calculate();
    QL_REQUIRE(couponLegNPV_ != Null<Rate>(), "coupon-leg NPV not available");
    return couponLegNPV_;
}

Real CreditDefaultSwap::defaultLegNPV() const {
    calculate();
    QL_REQUIRE(defaultLegNPV_ != Null<Rate>(), "default-leg NPV not available");
    return defaultLegNPV_;
}

Real CreditDefaultSwap::upfrontNPV() const {
    calculate();
    QL_REQUIRE(upfrontNPV_ != Null<Real>(), "upfront NPV not available");
    return upfrontNPV_;
}

Real CreditDefaultSwap::accrualRebateNPV() const {
    calculate();
    QL_REQUIRE(accrualRebateNPV_ != Null<Real>(), "accrual rebate NPV not available");
    return accrualRebateNPV_;
}

Real CreditDefaultSwap::upfrontBPS() const {
    calculate();
    QL_REQUIRE(upfrontBPS_ != Null<Real>(), "upfront BPS not available");
    return upfrontBPS_;
}

namespace {

class ObjectiveFunction {
public:
    ObjectiveFunction(Real target, SimpleQuote& quote, PricingEngine& engine, const CreditDefaultSwap::results* results)
        : target_(target), quote_(quote), engine_(engine), results_(results) {}

    Real operator()(Real guess) const {
        quote_.setValue(guess);
        engine_.calculate();
        return results_->value - target_;
    }

private:
    Real target_;
    SimpleQuote& quote_;
    PricingEngine& engine_;
    const CreditDefaultSwap::results* results_;
};
} // namespace

Rate CreditDefaultSwap::impliedHazardRate(Real targetNPV, const Handle<YieldTermStructure>& discountCurve,
                                          const DayCounter& dayCounter, Real recoveryRate, Real accuracy) const {

    boost::shared_ptr<SimpleQuote> flatRate(new SimpleQuote(0.0));

    Handle<DefaultProbabilityTermStructure> probability(boost::shared_ptr<DefaultProbabilityTermStructure>(
        new FlatHazardRate(0, WeekendsOnly(), Handle<Quote>(flatRate), dayCounter)));

    QuantExt::MidPointCdsEngine engine(probability, recoveryRate, discountCurve);
    setupArguments(engine.getArguments());
    const CreditDefaultSwap::results* results = dynamic_cast<const CreditDefaultSwap::results*>(engine.getResults());

    ObjectiveFunction f(targetNPV, *flatRate, engine, results);
    Rate guess = 0.001;
    Rate step = guess * 0.1;

    return Brent().solve(f, accuracy, guess, step);
}

Rate CreditDefaultSwap::conventionalSpread(Real conventionalRecovery, const Handle<YieldTermStructure>& discountCurve,
                                           const DayCounter& dayCounter) const {
    Rate flatHazardRate = impliedHazardRate(0.0, discountCurve, dayCounter, conventionalRecovery);

    Handle<DefaultProbabilityTermStructure> probability(boost::shared_ptr<DefaultProbabilityTermStructure>(
        new FlatHazardRate(0, WeekendsOnly(), flatHazardRate, dayCounter)));

    QuantExt::MidPointCdsEngine engine(probability, conventionalRecovery, discountCurve, true);
    setupArguments(engine.getArguments());
    engine.calculate();
    const CreditDefaultSwap::results* results = dynamic_cast<const CreditDefaultSwap::results*>(engine.getResults());
    return results->fairSpread;
}

const Date& CreditDefaultSwap::protectionStartDate() const { return protectionStart_; }

const Date& CreditDefaultSwap::protectionEndDate() const {
    return boost::dynamic_pointer_cast<Coupon>(leg_.back())->accrualEndDate();
}

CreditDefaultSwap::arguments::arguments() : side(Protection::Side(-1)), notional(Null<Real>()), spread(Null<Rate>()) {}

void CreditDefaultSwap::arguments::validate() const {
    QL_REQUIRE(side != Protection::Side(-1), "side not set");
    QL_REQUIRE(notional != Null<Real>(), "notional not set");
    QL_REQUIRE(notional != 0.0, "null notional set");
    QL_REQUIRE(spread != Null<Rate>(), "spread not set");
    QL_REQUIRE(!leg.empty(), "coupons not set");
    // upfront and accrual rebate can be empty to indicate there is no flow
    QL_REQUIRE(claim, "claim not set");
    QL_REQUIRE(protectionStart != Null<Date>(), "protection start date not set");
    QL_REQUIRE(maturity != Null<Date>(), "maturity date not set");
}

void CreditDefaultSwap::results::reset() {
    Instrument::results::reset();
    fairSpread = Null<Rate>();
    fairUpfront = Null<Rate>();
    couponLegBPS = Null<Real>();
    couponLegNPV = Null<Real>();
    defaultLegNPV = Null<Real>();
    upfrontBPS = Null<Real>();
    upfrontNPV = Null<Real>();
    accrualRebateNPV = Null<Real>();
}
} // namespace QuantExt
