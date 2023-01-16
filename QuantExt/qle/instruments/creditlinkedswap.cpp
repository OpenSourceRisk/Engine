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

#include <qle/instruments/creditlinkedswap.hpp>

#include <ql/event.hpp>

namespace QuantExt {

CreditLinkedSwap::CreditLinkedSwap(const std::vector<Leg>& legs, const std::vector<bool>& legPayers,
                                   const std::vector<LegType>& legTypes, const bool settlesAccrual,
                                   const Real fixedRecoveryRate,
                                   const QuantExt::CreditDefaultSwap::ProtectionPaymentTime& defaultPaymentTime,
                                   const Currency& currency)
    : legs_(legs), legPayers_(legPayers), legTypes_(legTypes), settlesAccrual_(settlesAccrual),
      fixedRecoveryRate_(fixedRecoveryRate), defaultPaymentTime_(defaultPaymentTime), currency_(currency) {
    QL_REQUIRE(legs_.size() == legPayers_.size(), "CreditLinkedSwap: legs size (" << legs_.size()
                                                                                  << ") must match legPayers size ("
                                                                                  << legPayers_.size() << ")");
    QL_REQUIRE(legs_.size() == legTypes_.size(), "CreditLinkedSwap: legs size (" << legs_.size()
                                                                                 << ") must match legTypes size ("
                                                                                 << legTypes_.size() << ")");
}

bool CreditLinkedSwap::isExpired() const { return QuantLib::detail::simple_event(maturity()).hasOccurred(); }

void CreditLinkedSwap::setupArguments(QuantLib::PricingEngine::arguments* args) const {
    CreditLinkedSwap::arguments* a = dynamic_cast<CreditLinkedSwap::arguments*>(args);
    QL_REQUIRE(a != nullptr, "CreditLinkedSwap::setupArguments(): wrong argument type");
    a->legs = legs_;
    a->legPayers = legPayers_;
    a->legTypes = legTypes_;
    a->settlesAccrual = settlesAccrual_;
    a->fixedRecoveryRate = fixedRecoveryRate_;
    a->defaultPaymentTime = defaultPaymentTime_;
    a->maturityDate = maturity();
    a->currency = currency_;
}

Date CreditLinkedSwap::maturity() const {
    Date maturity = Date::minDate();
    for (auto const& l : legs_) {
        for (auto const& c : l) {
            maturity = std::max(maturity, c->date());
        }
    }
    return maturity;
}

} // namespace QuantExt
