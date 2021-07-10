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
 Copyright (C) 2008 Chris Kenyon
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
#include <qle/termstructures/defaultprobabilityhelpers.hpp>

#include <ql/time/daycounters/actual360.hpp>
#include <ql/utilities/null_deleter.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

CdsHelper::CdsHelper(const Handle<Quote>& quote, const Period& tenor, Integer settlementDays, const Calendar& calendar,
                     Frequency frequency, BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
                     const DayCounter& dayCounter, Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
                     const Date& startDate, bool settlesAccrual,
                     CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                     const DayCounter& lastPeriodDayCounter)
    : RelativeDateDefaultProbabilityHelper(quote), tenor_(tenor), settlementDays_(settlementDays), calendar_(calendar),
      frequency_(frequency), paymentConvention_(paymentConvention), rule_(rule), dayCounter_(dayCounter),
      recoveryRate_(recoveryRate), discountCurve_(discountCurve), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime), lastPeriodDayCounter_(lastPeriodDayCounter),
      startDate_(startDate) {

    initializeDates();

    registerWith(discountCurve);
}

CdsHelper::CdsHelper(Rate quote, const Period& tenor, Integer settlementDays, const Calendar& calendar,
                     Frequency frequency, BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
                     const DayCounter& dayCounter, Real recoveryRate, const Handle<YieldTermStructure>& discountCurve,
                     const Date& startDate, bool settlesAccrual,
                     CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                     const DayCounter& lastPeriodDayCounter)
    : RelativeDateDefaultProbabilityHelper(quote), tenor_(tenor), settlementDays_(settlementDays), calendar_(calendar),
      frequency_(frequency), paymentConvention_(paymentConvention), rule_(rule), dayCounter_(dayCounter),
      recoveryRate_(recoveryRate), discountCurve_(discountCurve), settlesAccrual_(settlesAccrual),
      protectionPaymentTime_(protectionPaymentTime), lastPeriodDayCounter_(lastPeriodDayCounter),
      startDate_(startDate) {

    initializeDates();

    registerWith(discountCurve);
}

void CdsHelper::setTermStructure(DefaultProbabilityTermStructure* ts) {
    RelativeDateDefaultProbabilityHelper::setTermStructure(ts);

    probability_.linkTo(boost::shared_ptr<DefaultProbabilityTermStructure>(ts, null_deleter()), false);

    resetEngine();
}

void CdsHelper::update() {
    RelativeDateDefaultProbabilityHelper::update();
    resetEngine();
}

void CdsHelper::initializeDates() {

    // For CDS, the standard day counter is Actual/360 and the final period coupon accrual includes the maturity date.
    // If the main day counter is Act/360 and no lastPeriodDayCounter_ is given, default to Act/360 including last.
    Actual360 standardDayCounter;
    if (lastPeriodDayCounter_.empty()) {
        lastPeriodDayCounter_ = dayCounter_ == standardDayCounter ? Actual360(true) : dayCounter_;
    }

    protectionStart_ = evaluationDate_ + settlementDays_;

    Date startDate = startDate_ == Date() ? protectionStart_ : startDate_;
    // Only adjust start date if rule is not CDS or CDS2015. Unsure about OldCDS.
    if (rule_ != DateGeneration::CDS && rule_ != DateGeneration::CDS2015) {
        startDate = calendar_.adjust(startDate, paymentConvention_);
    }

    Date endDate;
    if (rule_ == DateGeneration::CDS2015 || rule_ == DateGeneration::CDS || rule_ == DateGeneration::OldCDS) {
        Date refDate = startDate_ == Date() ? evaluationDate_ : startDate_;
        endDate = cdsMaturity(refDate, tenor_, rule_);
    } else {
        // Keep the old logic here
        Date refDate = startDate_ == Date() ? protectionStart_ : startDate_ + settlementDays_;
        endDate = refDate + tenor_;
    }

    schedule_ = MakeSchedule()
                    .from(startDate)
                    .to(endDate)
                    .withFrequency(frequency_)
                    .withCalendar(calendar_)
                    .withConvention(paymentConvention_)
                    .withTerminationDateConvention(Unadjusted)
                    .withRule(rule_);
    earliestDate_ = schedule_.dates().front();
    latestDate_ = calendar_.adjust(schedule_.dates().back(), paymentConvention_);
}

SpreadCdsHelper::SpreadCdsHelper(const Handle<Quote>& runningSpread, const Period& tenor, Integer settlementDays,
                                 const Calendar& calendar, Frequency frequency, BusinessDayConvention paymentConvention,
                                 DateGeneration::Rule rule, const DayCounter& dayCounter, Real recoveryRate,
                                 const Handle<YieldTermStructure>& discountCurve, const Date& startDate,
                                 bool settlesAccrual, CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                                 const DayCounter& lastPeriodDayCounter)
    : CdsHelper(runningSpread, tenor, settlementDays, calendar, frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, startDate, settlesAccrual, protectionPaymentTime, lastPeriodDayCounter) {}

SpreadCdsHelper::SpreadCdsHelper(Rate runningSpread, const Period& tenor, Integer settlementDays,
                                 const Calendar& calendar, Frequency frequency, BusinessDayConvention paymentConvention,
                                 DateGeneration::Rule rule, const DayCounter& dayCounter, Real recoveryRate,
                                 const Handle<YieldTermStructure>& discountCurve, const Date& startDate,
                                 bool settlesAccrual, CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                                 const DayCounter& lastPeriodDayCounter)
    : CdsHelper(runningSpread, tenor, settlementDays, calendar, frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, startDate, settlesAccrual, protectionPaymentTime, lastPeriodDayCounter) {}

Real SpreadCdsHelper::impliedQuote() const {
    swap_->recalculate();
    return swap_->fairSpread();
}

void SpreadCdsHelper::resetEngine() {

    swap_ = boost::make_shared<QuantExt::CreditDefaultSwap>(
        Protection::Buyer, 100.0, 0.01, schedule_, paymentConvention_, dayCounter_, settlesAccrual_,
        protectionPaymentTime_, protectionStart_, boost::shared_ptr<Claim>(), lastPeriodDayCounter_,
        evaluationDate_);

    swap_->setPricingEngine(
        boost::make_shared<QuantExt::MidPointCdsEngine>(probability_, recoveryRate_, discountCurve_));
}

UpfrontCdsHelper::UpfrontCdsHelper(const Handle<Quote>& upfront, Rate runningSpread, const Period& tenor,
                                   Integer settlementDays, const Calendar& calendar, Frequency frequency,
                                   BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
                                   const DayCounter& dayCounter, Real recoveryRate,
                                   const Handle<YieldTermStructure>& discountCurve, const Date& startDate,
                                   Natural upfrontSettlementDays, bool settlesAccrual,
                                   CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                                   const DayCounter& lastPeriodDayCounter)
    : CdsHelper(upfront, tenor, settlementDays, calendar, frequency, paymentConvention, rule, dayCounter, recoveryRate,
                discountCurve, startDate, settlesAccrual, protectionPaymentTime, lastPeriodDayCounter),
      upfrontSettlementDays_(upfrontSettlementDays), runningSpread_(runningSpread) {
    initializeUpfront();
}

UpfrontCdsHelper::UpfrontCdsHelper(Rate upfrontSpread, Rate runningSpread, const Period& tenor, Integer settlementDays,
                                   const Calendar& calendar, Frequency frequency,
                                   BusinessDayConvention paymentConvention, DateGeneration::Rule rule,
                                   const DayCounter& dayCounter, Real recoveryRate,
                                   const Handle<YieldTermStructure>& discountCurve, const Date& startDate,
                                   Natural upfrontSettlementDays, bool settlesAccrual,
                                   CreditDefaultSwap::ProtectionPaymentTime protectionPaymentTime,
                                   const DayCounter& lastPeriodDayCounter)
    : CdsHelper(upfrontSpread, tenor, settlementDays, calendar, frequency, paymentConvention, rule, dayCounter,
                recoveryRate, discountCurve, startDate, settlesAccrual, protectionPaymentTime, lastPeriodDayCounter),
      upfrontSettlementDays_(upfrontSettlementDays), runningSpread_(runningSpread) {
    initializeUpfront();
}

void UpfrontCdsHelper::initializeUpfront() {
    upfrontDate_ = calendar_.advance(evaluationDate_, upfrontSettlementDays_, Days, paymentConvention_);
}

Real UpfrontCdsHelper::impliedQuote() const {
    SavedSettings backup;
    Settings::instance().includeTodaysCashFlows() = true;
    swap_->recalculate();
    return swap_->fairUpfront();
}

void UpfrontCdsHelper::resetEngine() {
    swap_ = boost::make_shared<CreditDefaultSwap>(
        Protection::Buyer, 100.0, 0.01, runningSpread_, schedule_, paymentConvention_, dayCounter_, settlesAccrual_,
        protectionPaymentTime_, protectionStart_, upfrontDate_, boost::shared_ptr<Claim>(), lastPeriodDayCounter_,
        evaluationDate_);
    swap_->setPricingEngine(
        boost::make_shared<QuantExt::MidPointCdsEngine>(probability_, recoveryRate_, discountCurve_, true));
}
} // namespace QuantExt
