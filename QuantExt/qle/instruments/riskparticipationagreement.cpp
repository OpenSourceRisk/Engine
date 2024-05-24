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

#include <qle/instruments/riskparticipationagreement.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/event.hpp>

namespace QuantExt {

RiskParticipationAgreement::RiskParticipationAgreement(
    const std::vector<Leg>& underlying, const std::vector<bool>& underlyingPayer,
    const std::vector<std::string>& underlyingCcys, const std::vector<Leg>& protectionFee,
    const bool protectionFeePayer, const std::vector<std::string>& protectionFeeCcys, const Real participationRate,
    const Date& protectionStart, const Date& protectionEnd, const bool settlesAccrual, const Real fixedRecoveryRate,
    const QuantLib::ext::shared_ptr<Exercise>& exercise, const bool exerciseIsLong, const std::vector<QuantLib::ext::shared_ptr<CashFlow>>& premium, const bool nakedOption)
    : underlying_(underlying), underlyingPayer_(underlyingPayer), underlyingCcys_(underlyingCcys),
      protectionFee_(protectionFee), protectionFeePayer_(protectionFeePayer), protectionFeeCcys_(protectionFeeCcys),
      participationRate_(participationRate), protectionStart_(protectionStart), protectionEnd_(protectionEnd),
      settlesAccrual_(settlesAccrual), fixedRecoveryRate_(fixedRecoveryRate), exercise_(exercise),
      exerciseIsLong_(exerciseIsLong), premium_(premium), nakedOption_(nakedOption) {

    QL_REQUIRE(underlying_.size() == underlyingPayer_.size(),
               "underlying size (" << underlying_.size() << ") must match underlying payer size ("
                                   << underlyingPayer_.size() << ")");
    QL_REQUIRE(underlying_.size() == underlyingCcys_.size(),
               "underlying size (" << underlying_.size() << ") must match underlying ccys size ("
                                   << underlyingCcys_.size() << ")");
    QL_REQUIRE(!underlying_.empty(), "underlying is empty");
    QL_REQUIRE(protectionFee_.size() == protectionFeeCcys_.size(),
               "protection fee size (" << protectionFee_.size() << ") must match protecttion fee ccys size ("
                                       << protectionFeeCcys_.size() << ")");
    QL_REQUIRE(participationRate_ > 0.0 || close_enough(participationRate_, 0.0),
               "participation rate must be non-negative (" << participationRate_ << ")");
    QL_REQUIRE(protectionEnd_ > protectionStart_,
               "protection end (" << protectionEnd_ << ") must be greater than protection start " << protectionStart_);

    // the maturity is the maximum of the protection end date and the last fee cashflow payment date
    maturity_ = protectionEnd_;
    for (auto const& p : protectionFee) {
        if (!p.empty())
            maturity_ = std::max(maturity_, CashFlows::maturityDate(p));
    }

    // the underlying maturity is the maturity over the underlying legs
    underlyingMaturity_ = Date::minDate();
    for (auto const& l : underlying)
        underlyingMaturity_ = std::max(underlyingMaturity_, CashFlows::maturityDate(l));

    for (auto const& l : underlying) {
        for (auto const& c : l) {
            registerWith(c);
            if (auto lazy = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c))
                lazy->alwaysForwardNotifications();
        }
    }

    for (auto const& l : protectionFee) {
        for (auto const& c : l) {
            registerWith(c);
            if (auto lazy = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c))
                lazy->alwaysForwardNotifications();
        }
    }
}

bool RiskParticipationAgreement::isExpired() const { return detail::simple_event(maturity()).hasOccurred(); }

void RiskParticipationAgreement::setupExpired() const {
    Instrument::setupExpired();
    optionRepresentation_.clear();
    optionMultiplier_.clear();
    optionRepresentationPeriods_.clear();
    optionRepresentationReferenceDate_ = Date();
}

void RiskParticipationAgreement::setupArguments(QuantLib::PricingEngine::arguments* args) const {
    RiskParticipationAgreement::arguments* arguments = dynamic_cast<RiskParticipationAgreement::arguments*>(args);
    QL_REQUIRE(arguments, "RiskParticipationAgreement::setupArguments(): wrong argument type");
    arguments->underlying = underlying_;
    arguments->underlyingPayer = underlyingPayer_;
    arguments->underlyingCcys = underlyingCcys_;
    arguments->protectionFee = protectionFee_;
    arguments->protectionFeePayer = protectionFeePayer_;
    arguments->protectionFeeCcys = protectionFeeCcys_;
    arguments->participationRate = participationRate_;
    arguments->protectionStart = protectionStart_;
    arguments->protectionEnd = protectionEnd_;
    arguments->underlyingMaturity = underlyingMaturity_;
    arguments->settlesAccrual = settlesAccrual_;
    arguments->fixedRecoveryRate = fixedRecoveryRate_;
    arguments->exercise = exercise_;
    arguments->exerciseIsLong = exerciseIsLong_;
    arguments->premium = premium_;
    arguments->nakedOption = nakedOption_;
    // provide previously computed option representation, if this is available
    arguments->optionRepresentation = optionRepresentation_;
    arguments->optionMultiplier = optionMultiplier_;
    arguments->optionRepresentationPeriods = optionRepresentationPeriods_;
    arguments->optionRepresentationReferenceDate = optionRepresentationReferenceDate_;
}

void RiskParticipationAgreement::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const RiskParticipationAgreement::results* results = dynamic_cast<const RiskParticipationAgreement::results*>(r);
    QL_REQUIRE(results, "RiskParticipationAgreement::fetchResults(): wrong result type");
    // might be empty / null if engine does not provide these
    optionRepresentation_ = results->optionRepresentation;
    optionMultiplier_ = results->optionMultiplier;
    optionRepresentationPeriods_ = results->optionRepresentationPeriods;
    optionRepresentationReferenceDate_ = results->optionRepresentationReferenceDate;
}

} // namespace QuantExt
