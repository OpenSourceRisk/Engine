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

//#include <ql/experimental/credit/syntheticcdo.hpp>
#include <qle/instruments/syntheticcdo.hpp>

#ifndef QL_PATCH_SOLARIS

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/event.hpp>
#include <ql/math/solvers1d/brent.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
//#include <ql/experimental/credit/gaussianlhplossmodel.hpp>
#include <qle/models/gaussianlhplossmodel.hpp>
//#include <ql/experimental/credit/midpointcdoengine.hpp>
#include <qle/pricingengines/midpointcdoengine.hpp>

using namespace std;
using namespace QuantLib;

namespace QuantExt {

SyntheticCDO::SyntheticCDO(const QuantLib::ext::shared_ptr<QuantExt::Basket>& basket, Protection::Side side,
                           const Schedule& schedule, Rate upfrontRate, Rate runningRate, const DayCounter& dayCounter,
                           BusinessDayConvention paymentConvention, bool settlesAccrual,
                           CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime, Date protectionStart,
                           Date upfrontDate, boost::optional<Real> notional, Real recoveryRate,
                           const DayCounter& lastPeriodDayCounter)
    : basket_(basket), side_(side), upfrontRate_(upfrontRate), runningRate_(runningRate),
      leverageFactor_(notional ? notional.get() / basket->trancheNotional() : 1.), dayCounter_(dayCounter),
      paymentConvention_(paymentConvention), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime),
      protectionStart_(protectionStart == Null<Date>() ? schedule[0] : protectionStart),
      recoveryRate_(recoveryRate) {
    QL_REQUIRE((schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) ||
                   protectionStart_ <= schedule[0],
               "protection can not start after accrual for (pre big bang-) CDS");
    QL_REQUIRE(basket->names().size() > 0, "basket is empty");
    // Basket inception must lie before contract protection start.
    QL_REQUIRE(basket->refDate() <= schedule.startDate(),
               // using the start date of the schedule might be wrong, think of the
               //  CDS rule
               "Basket did not exist before contract start.");

    // Notice the notional is that of the basket at basket inception, some
    //   names might have defaulted in between
    normalizedLeg_ = FixedRateLeg(schedule)
                         .withNotionals(basket_->trancheNotional() * leverageFactor_)
                         .withCouponRates(runningRate, dayCounter)
                         .withPaymentAdjustment(paymentConvention)
                         .withLastPeriodDayCounter(lastPeriodDayCounter);

    // If empty, adjust to T+3 standard settlement
    Date effectiveUpfrontDate =
        upfrontDate == Null<Date>()
            ? schedule.calendar().advance(schedule.calendar().adjust(protectionStart_, paymentConvention), 3, Days,
                                          paymentConvention)
            : upfrontDate;
    // '2' is used above since the protection start is assumed to be
    // on trade_date + 1
    upfrontPayment_.reset(
        new SimpleCashFlow(basket_->trancheNotional() * leverageFactor_ * upfrontRate, effectiveUpfrontDate));

    QL_REQUIRE(upfrontPayment_->date() >= protectionStart_, "upfront can not be due before contract start");

    if (schedule.rule() == DateGeneration::CDS || schedule.rule() == DateGeneration::CDS2015) {
            accrualRebate_= QuantLib::ext::make_shared<SimpleCashFlow>(QuantLib::CashFlows::accruedAmount(normalizedLeg_, false, protectionStart_+1),
                                                               effectiveUpfrontDate);      
            Date current = std::max((Date)Settings::instance().evaluationDate(), protectionStart_);
            accrualRebateCurrent_ = QuantLib::ext::make_shared<SimpleCashFlow>(
                CashFlows::accruedAmount(normalizedLeg_, false, current + 1),
                schedule.calendar().advance(current, 3, Days, paymentConvention));

    }

   

    // register with probabilities if the corresponding issuer is, baskets
    //   are not registered with the DTS
    for (Size i = 0; i < basket->names().size(); i++) {
        /* This turns out to be a problem: depends on today but I am not
        modifying the registrations, if we go back in time in the
        calculations this would left me unregistered to some. Not impossible
        to de-register and register when updating but i am dropping it.

        if(!basket->pool()->get(basket->names()[i]).
            defaultedBetween(schedule.dates()[0], today,
                                 basket->pool()->defaultKeys()[i]))
        */
        // registers with the associated curve (issuer and event type)
        // \todo make it possible to access them by name instead of index
        registerWith(basket->pool()->get(basket->names()[i]).defaultProbability(basket->pool()->defaultKeys()[i]));
        /* \todo Issuers should be observables/obsrvr and they would in turn
        regiter with the DTS; only we might get updates from curves we do
        not use.
        */
    }
    registerWith(basket_);
}

Rate SyntheticCDO::premiumValue() const {
    calculate();
    return premiumValue_;
}

Rate SyntheticCDO::protectionValue() const {
    calculate();
    return protectionValue_;
}

Real SyntheticCDO::premiumLegNPV() const {
    calculate();
    if (side_ == Protection::Buyer)
        return premiumValue_;
    return -premiumValue_;
}

Real SyntheticCDO::protectionLegNPV() const {
    calculate();
    if (side_ == Protection::Buyer)
        return -protectionValue_;
    return premiumValue_;
}

Rate SyntheticCDO::fairPremium() const {
    calculate();
    return runningRate_ * (protectionValue_ - upfrontPremiumValue_) / premiumValue_;
}

Rate SyntheticCDO::fairUpfrontPremium() const {
    calculate();
    return (protectionValue_ - premiumValue_) / remainingNotional_;
}

vector<Real> SyntheticCDO::expectedTrancheLoss() const {
    calculate();
    return expectedTrancheLoss_;
}

Size SyntheticCDO::error() const {
    calculate();
    return error_;
}

bool SyntheticCDO::isExpired() const {
    // FIXME: it could have also expired (knocked out) because theres
    //   no remaining tranche notional.
    return detail::simple_event(normalizedLeg_.back()->date()).hasOccurred();
}

Real SyntheticCDO::remainingNotional() const {
    calculate();
    return remainingNotional_;
}

void SyntheticCDO::setupArguments(PricingEngine::arguments* args) const {
    SyntheticCDO::arguments* arguments = dynamic_cast<SyntheticCDO::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type");
    arguments->basket = basket_;
    arguments->side = side_;
    arguments->normalizedLeg = normalizedLeg_;

    arguments->upfrontRate = upfrontRate_;
    arguments->runningRate = runningRate_;
    arguments->dayCounter = dayCounter_;
    arguments->paymentConvention = paymentConvention_;
    arguments->leverageFactor = leverageFactor_;
    arguments->upfrontPayment = upfrontPayment_;
    arguments->accrualRebate = accrualRebate_;
    arguments->accrualRebateCurrent = accrualRebateCurrent_;
    arguments->settlesAccrual = settlesAccrual_;
    arguments->protectionPaymentTime = protectionPaymentTime_;
    arguments->protectionStart = protectionStart_;
    arguments->maturity = maturity();
    arguments->recoveryRate = recoveryRate_;
}

void SyntheticCDO::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);

    const SyntheticCDO::results* results = dynamic_cast<const SyntheticCDO::results*>(r);
    QL_REQUIRE(results != 0, "wrong result type");

    premiumValue_ = results->premiumValue;
    protectionValue_ = results->protectionValue;
    upfrontPremiumValue_ = results->upfrontPremiumValue;
    remainingNotional_ = results->remainingNotional;
    error_ = results->error;
    expectedTrancheLoss_ = results->expectedTrancheLoss;
}

void SyntheticCDO::setupExpired() const {
    Instrument::setupExpired();
    premiumValue_ = 0.0;
    protectionValue_ = 0.0;
    upfrontPremiumValue_ = 0.0;
    remainingNotional_ = 1.0;
    expectedTrancheLoss_.clear();
}

void SyntheticCDO::arguments::validate() const {
    QL_REQUIRE(side != Protection::Side(-1), "side not set");
    QL_REQUIRE(basket && !basket->names().empty(), "no basket given");
    QL_REQUIRE(runningRate != Null<Real>(), "no premium rate given");
    QL_REQUIRE(upfrontRate != Null<Real>(), "no upfront rate given");
    QL_REQUIRE(!dayCounter.empty(), "no day counter given");
}

void SyntheticCDO::results::reset() {
    Instrument::results::reset();
    premiumValue = Null<Real>();
    protectionValue = Null<Real>();
    upfrontPremiumValue = Null<Real>();
    remainingNotional = Null<Real>();
    error = 0;
    expectedTrancheLoss.clear();
}

namespace {

class ObjectiveFunction {
public:
    ObjectiveFunction(Real target, SimpleQuote& quote, PricingEngine& engine, const SyntheticCDO::results* results)
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
    const SyntheticCDO::results* results_;
};
} // namespace

// untested, not sure this is not messing up, once it comes out of this
//   the basket model is different.....
Real SyntheticCDO::implicitCorrelation(const std::vector<Real>& recoveries,
                                       const Handle<YieldTermStructure>& discountCurve, Real targetNPV,
                                       Real accuracy) const {
    QuantLib::ext::shared_ptr<SimpleQuote> correl(new SimpleQuote(0.0));

    QuantLib::ext::shared_ptr<GaussianLHPLossModel> lhp(new GaussianLHPLossModel(Handle<Quote>(correl), recoveries));

    // lock
    basket_->setLossModel(lhp);

    QuantExt::MidPointCDOEngine engineIC(discountCurve);
    setupArguments(engineIC.getArguments());
    const SyntheticCDO::results* results = dynamic_cast<const SyntheticCDO::results*>(engineIC.getResults());

    // aviod recal of the basket on engine updates through the quote
    basket_->recalculate();
    basket_->freeze();

    ObjectiveFunction f(targetNPV, *correl, engineIC, results);
    Rate guess = 0.001;
    //  Rate step = guess*0.1;

    // wrap/catch to be able to unfreeze the basket:
    Real solution = Brent().solve(f, accuracy, guess, QL_EPSILON, 1. - QL_EPSILON);
    basket_->unfreeze();
    return solution;
}
} // namespace QuantExt

#endif
