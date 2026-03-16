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

#include <qle/instruments/riskparticipationagreement_tlock.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/event.hpp>

namespace QuantExt {

RiskParticipationAgreementTLock::RiskParticipationAgreementTLock(
    const QuantLib::ext::shared_ptr<QuantLib::Bond>& bond, Real bondNotional, bool payer, Real referenceRate,
    const DayCounter& dayCounter, const Date& terminationDate, const Date& paymentDate,
    const std::vector<Leg>& protectionFee, const bool protectionFeePayer,
    const std::vector<std::string>& protectionFeeCcys, const Real participationRate, const Date& protectionStart,
    const Date& protectionEnd, const bool settlesAccrual, const Real fixedRecoveryRate)
    : bond_(bond), bondNotional_(bondNotional), payer_(payer), referenceRate_(referenceRate), dayCounter_(dayCounter),
      terminationDate_(terminationDate), paymentDate_(paymentDate), protectionFee_(protectionFee),
      protectionFeePayer_(protectionFeePayer), protectionFeeCcys_(protectionFeeCcys),
      participationRate_(participationRate), protectionStart_(protectionStart), protectionEnd_(protectionEnd),
      settlesAccrual_(settlesAccrual), fixedRecoveryRate_(fixedRecoveryRate) {

    // checks

    QL_REQUIRE(bond != nullptr, "RiskParticipationAgreementTLock: underlying bond is null");
    QL_REQUIRE(!dayCounter.empty(), "RiskParticipationAgreementTLock: day counter is empty");

    QL_REQUIRE(paymentDate_ >= terminationDate, "RiskParticipationAgreementTLock: payment date ("
                                                    << paymentDate_ << ") must be >= termination date ("
                                                    << terminationDate_ << ")");
    QL_REQUIRE(protectionFee_.size() == protectionFeeCcys_.size(),
               "protection fee size (" << protectionFee_.size() << ") must match protecttion fee ccys size ("
                                       << protectionFeeCcys_.size() << ")");
    QL_REQUIRE(participationRate_ > 0.0 || close_enough(participationRate_, 0.0),
               "participation rate must be non-negative (" << participationRate_ << ")");
    QL_REQUIRE(protectionEnd_ > protectionStart_,
               "protection end (" << protectionEnd_ << ") must be greater than protection start " << protectionStart_);

    for (auto const& c : bond->cashflows()) {
        if (auto tmp = QuantLib::ext::dynamic_pointer_cast<Coupon>(c)) {
            QL_REQUIRE(QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(c),
                       "RiskParticipationAgreementTLock: only fixed rate coupons allowed in bond underlying");
        }
    }

    // the maturity is the maximum of the protection end date and fee payment dates

    maturity_ = protectionEnd_;
    for (auto const& p : protectionFee)
        maturity_ = std::max(maturity_, CashFlows::maturityDate(p));

    // register with observables

    for (auto const& l : protectionFee)
        for (auto const& c : l)
            registerWith(c);
}

bool RiskParticipationAgreementTLock::isExpired() const { return detail::simple_event(maturity_).hasOccurred(); }

void RiskParticipationAgreementTLock::setupExpired() const { Instrument::setupExpired(); }

void RiskParticipationAgreementTLock::setupArguments(QuantLib::PricingEngine::arguments* args) const {
    RiskParticipationAgreementTLock::arguments* arguments =
        dynamic_cast<RiskParticipationAgreementTLock::arguments*>(args);
    QL_REQUIRE(arguments, "RiskParticipationAgreement::setupArguments(): wrong argument type");
    arguments->bond = bond_;
    arguments->bondNotional = bondNotional_;
    arguments->payer = payer_;
    arguments->referenceRate = referenceRate_;
    arguments->dayCounter = dayCounter_;
    arguments->terminationDate = terminationDate_;
    arguments->paymentDate = paymentDate_;

    arguments->protectionFee = protectionFee_;
    arguments->protectionFeePayer = protectionFeePayer_;
    arguments->protectionFeeCcys = protectionFeeCcys_;
    arguments->participationRate = participationRate_;
    arguments->protectionStart = protectionStart_;
    arguments->protectionEnd = protectionEnd_;
    arguments->settlesAccrual = settlesAccrual_;
    arguments->fixedRecoveryRate = fixedRecoveryRate_;
}

void RiskParticipationAgreementTLock::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
}

} // namespace QuantExt
