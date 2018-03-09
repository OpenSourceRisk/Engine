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

#include <ql/pricingengines/swap/discountingswapengine.hpp>
#include <ql/time/calendars/weekendsonly.hpp>

#include <qle/instruments/makeaverageois.hpp>

namespace QuantExt {

MakeAverageOIS::MakeAverageOIS(const Period& swapTenor, const boost::shared_ptr<OvernightIndex>& overnightIndex,
                               const Period& onTenor, Rate fixedRate, const Period& fixedTenor,
                               const DayCounter& fixedDayCounter, const Period& spotLagTenor,
                               const Period& forwardStart)
    : swapTenor_(swapTenor), overnightIndex_(overnightIndex), onTenor_(onTenor), fixedRate_(fixedRate),
      fixedTenor_(fixedTenor), fixedDayCounter_(fixedDayCounter), spotLagTenor_(spotLagTenor),
      forwardStart_(forwardStart), type_(AverageOIS::Receiver), nominal_(1.0), effectiveDate_(Date()),
      spotLagCalendar_(overnightIndex->fixingCalendar()), fixedCalendar_(WeekendsOnly()), fixedConvention_(Unadjusted),
      fixedTerminationDateConvention_(Unadjusted), fixedRule_(DateGeneration::Backward), fixedEndOfMonth_(false),
      fixedFirstDate_(Date()), fixedNextToLastDate_(Date()),
      fixedPaymentAdjustment_(overnightIndex->businessDayConvention()),
      fixedPaymentCalendar_(overnightIndex->fixingCalendar()), onCalendar_(overnightIndex->fixingCalendar()),
      onConvention_(overnightIndex->businessDayConvention()),
      onTerminationDateConvention_(overnightIndex->businessDayConvention()), onRule_(DateGeneration::Backward),
      onEndOfMonth_(false), onFirstDate_(Date()), onNextToLastDate_(Date()), rateCutoff_(0), onSpread_(0.0),
      onGearing_(1.0), onDayCounter_(overnightIndex->dayCounter()),
      onPaymentAdjustment_(overnightIndex->businessDayConvention()),
      onPaymentCalendar_(overnightIndex->fixingCalendar()),
      onCouponPricer_(boost::make_shared<AverageONIndexedCouponPricer>()) {}

MakeAverageOIS::operator AverageOIS() const {
    boost::shared_ptr<AverageOIS> swap = *this;
    return *swap;
}

MakeAverageOIS::operator boost::shared_ptr<AverageOIS>() const {

    // Deduce the effective date if it is not given.
    Date effectiveDate;
    if (effectiveDate_ != Date()) {
        effectiveDate = effectiveDate_;
    } else {
        Date valuationDate = Settings::instance().evaluationDate();
        // if the evaluation date is not a business day
        // then move to the next business day
        valuationDate = spotLagCalendar_.adjust(valuationDate);
        Date spotDate = spotLagCalendar_.advance(valuationDate, spotLagTenor_);
        effectiveDate = spotDate + forwardStart_;
    }

    // Deduce the termination date if it is not given.
    Date terminationDate;
    if (terminationDate_ != Date()) {
        terminationDate = terminationDate_;
    } else {
        terminationDate = effectiveDate + swapTenor_;
    }

    Schedule fixedSchedule(effectiveDate, terminationDate, fixedTenor_, fixedCalendar_, fixedConvention_,
                           fixedTerminationDateConvention_, fixedRule_, fixedEndOfMonth_, fixedFirstDate_,
                           fixedNextToLastDate_);

    Schedule onSchedule(effectiveDate, terminationDate, onTenor_, onCalendar_, onConvention_,
                        onTerminationDateConvention_, onRule_, onEndOfMonth_, onFirstDate_, onNextToLastDate_);

    boost::shared_ptr<AverageOIS> swap(
        new AverageOIS(type_, nominal_, fixedSchedule, fixedRate_, fixedDayCounter_, fixedPaymentAdjustment_,
                       fixedPaymentCalendar_, onSchedule, overnightIndex_, onPaymentAdjustment_, onPaymentCalendar_,
                       rateCutoff_, onSpread_, onGearing_, onDayCounter_, onCouponPricer_));

    swap->setPricingEngine(engine_);
    return swap;
}

MakeAverageOIS& MakeAverageOIS::receiveFixed(bool receiveFixed) {
    type_ = receiveFixed ? AverageOIS::Receiver : AverageOIS::Payer;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withType(AverageOIS::Type type) {
    type_ = type;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withNominal(Real nominal) {
    nominal_ = nominal;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withEffectiveDate(const Date& effectiveDate) {
    effectiveDate_ = effectiveDate;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withTerminationDate(const Date& terminationDate) {
    terminationDate_ = terminationDate;
    swapTenor_ = Period();
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withRule(DateGeneration::Rule rule) {
    fixedRule_ = rule;
    onRule_ = rule;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withSpotLagCalendar(const Calendar& spotLagCalendar) {
    spotLagCalendar_ = spotLagCalendar;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedCalendar(const Calendar& fixedCalendar) {
    fixedCalendar_ = fixedCalendar;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedConvention(BusinessDayConvention fixedConvention) {
    fixedConvention_ = fixedConvention;
    return *this;
}

MakeAverageOIS&
MakeAverageOIS::withFixedTerminationDateConvention(BusinessDayConvention fixedTerminationDateConvention) {
    fixedTerminationDateConvention_ = fixedTerminationDateConvention;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedRule(DateGeneration::Rule fixedRule) {
    fixedRule_ = fixedRule;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedEndOfMonth(bool fixedEndOfMonth) {
    fixedEndOfMonth_ = fixedEndOfMonth;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedFirstDate(const Date& fixedFirstDate) {
    fixedFirstDate_ = fixedFirstDate;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedNextToLastDate(const Date& fixedNextToLastDate) {
    fixedNextToLastDate_ = fixedNextToLastDate;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedPaymentAdjustment(BusinessDayConvention fixedPaymentAdjustment) {
    fixedPaymentAdjustment_ = fixedPaymentAdjustment;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withFixedPaymentCalendar(const Calendar& fixedPaymentCalendar) {
    fixedPaymentCalendar_ = fixedPaymentCalendar;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONCalendar(const Calendar& onCalendar) {
    onCalendar_ = onCalendar;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONConvention(BusinessDayConvention onConvention) {
    onConvention_ = onConvention;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONTerminationDateConvention(BusinessDayConvention onTerminationDateConvention) {
    onTerminationDateConvention_ = onTerminationDateConvention;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONRule(DateGeneration::Rule onRule) {
    onRule_ = onRule;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONEndOfMonth(bool onEndOfMonth) {
    onEndOfMonth_ = onEndOfMonth;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONFirstDate(const Date& onFirstDate) {
    onFirstDate_ = onFirstDate;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONNextToLastDate(const Date& onNextToLastDate) {
    onNextToLastDate_ = onNextToLastDate;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withRateCutoff(Natural rateCutoff) {
    rateCutoff_ = rateCutoff;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONSpread(Spread onSpread) {
    onSpread_ = onSpread;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONGearing(Real onGearing) {
    onGearing_ = onGearing;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONDayCounter(const DayCounter& onDayCounter) {
    onDayCounter_ = onDayCounter;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONPaymentAdjustment(BusinessDayConvention onPaymentAdjustment) {
    onPaymentAdjustment_ = onPaymentAdjustment;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withONPaymentCalendar(const Calendar& onPaymentCalendar) {
    onPaymentCalendar_ = onPaymentCalendar;
    return *this;
}

MakeAverageOIS&
MakeAverageOIS::withONCouponPricer(const boost::shared_ptr<AverageONIndexedCouponPricer>& onCouponPricer) {
    onCouponPricer_ = onCouponPricer;
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withDiscountingTermStructure(const Handle<YieldTermStructure>& discountCurve) {
    bool includeSettlementDateFlows = false;
    engine_ = boost::shared_ptr<PricingEngine>(new DiscountingSwapEngine(discountCurve, includeSettlementDateFlows));
    return *this;
}

MakeAverageOIS& MakeAverageOIS::withPricingEngine(const boost::shared_ptr<PricingEngine>& engine) {
    engine_ = engine;
    return *this;
}
} // namespace QuantExt
