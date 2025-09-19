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

#include <ql/event.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <qle/instruments/forwardbond.hpp>
#include <qle/pricingengines/discountingforwardbondengine.hpp>

namespace QuantExt {

ForwardBond::ForwardBond(const QuantLib::ext::shared_ptr<Bond>& underlying, const Real strikeAmount,
                         const Date& fwdMaturityDate, const Date& fwdSettlementDate, const bool isPhysicallySettled,
                         const bool knockOut, const bool settlementDirty, const Real compensationPayment,
                         const Date compensationPaymentDate, const bool isLong, const Real bondNotional)
    : underlying_(underlying), strikeAmount_(strikeAmount), lockRate_(Null<Real>()), fwdMaturityDate_(fwdMaturityDate),
      fwdSettlementDate_(fwdSettlementDate), isPhysicallySettled_(isPhysicallySettled), knockOut_(knockOut),
      settlementDirty_(settlementDirty), compensationPayment_(compensationPayment),
      compensationPaymentDate_(compensationPaymentDate), isLong_(isLong), bondNotional_(bondNotional),
      dv01_(Null<Real>()) {
    registerWith(underlying);
}

ForwardBond::ForwardBond(const QuantLib::ext::shared_ptr<Bond>& underlying, const Real lockRate,
                         const DayCounter& lockRateDayCounter, const bool longInForward, const Date& fwdMaturityDate,
                         const Date& fwdSettlementDate, const bool isPhysicallySettled, const bool knockOut,
                         const bool settlementDirty, const Real compensationPayment, const Date compensationPaymentDate,
                         const bool isLong, const Real bondNotional, const Real dv01)
    : underlying_(underlying), strikeAmount_(Null<Real>()), lockRate_(lockRate),
      lockRateDayCounter_(lockRateDayCounter), fwdMaturityDate_(fwdMaturityDate), fwdSettlementDate_(fwdSettlementDate),
      isPhysicallySettled_(isPhysicallySettled), knockOut_(knockOut), settlementDirty_(settlementDirty),
      compensationPayment_(compensationPayment), compensationPaymentDate_(compensationPaymentDate), isLong_(isLong),
      bondNotional_(bondNotional), dv01_(dv01) {
    registerWith(underlying);
}

bool ForwardBond::isExpired() const {
    ext::optional<bool> includeToday = Settings::instance().includeTodaysCashFlows();
    Date refDate = Settings::instance().evaluationDate();
    return detail::simple_event(fwdMaturityDate_).hasOccurred(refDate, includeToday);
}

void ForwardBond::setupArguments(PricingEngine::arguments* args) const {
    ForwardBond::arguments* arguments = dynamic_cast<ForwardBond::arguments*>(args);
    QL_REQUIRE(arguments != 0, "wrong argument type in forward bond");
    arguments->underlying = underlying_;
    arguments->strikeAmount = strikeAmount_;
    arguments->lockRate = lockRate_;
    arguments->lockRateDayCounter = lockRateDayCounter_;
    arguments->fwdMaturityDate = fwdMaturityDate_;
    arguments->fwdSettlementDate = fwdSettlementDate_;
    arguments->isPhysicallySettled = isPhysicallySettled_;
    arguments->knockOut = knockOut_;
    arguments->settlementDirty = settlementDirty_;
    arguments->compensationPayment = compensationPayment_;
    arguments->compensationPaymentDate = compensationPaymentDate_;
    arguments->isLong = isLong_;
    arguments->bondNotional = bondNotional_;
    arguments->dv01 = dv01_;
}

void ForwardBond::results::reset() {
    Instrument::results::reset();
}

void ForwardBond::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const ForwardBond::results* results = dynamic_cast<const ForwardBond::results*>(r);
    QL_REQUIRE(results, "wrong result type");
}

void ForwardBond::arguments::validate() const { QL_REQUIRE(underlying, "bond pointer is null"); }

} // namespace QuantExt
