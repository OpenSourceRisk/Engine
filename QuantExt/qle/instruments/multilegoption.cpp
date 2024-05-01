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

#include <qle/instruments/multilegoption.hpp>

#include <ql/cashflows/floatingratecoupon.hpp>

namespace QuantExt {

MultiLegOption::MultiLegOption(const std::vector<Leg>& legs, const std::vector<bool>& payer,
                               const std::vector<Currency>& currency, const QuantLib::ext::shared_ptr<Exercise>& exercise,
                               const Settlement::Type settlementType, Settlement::Method settlementMethod)
    : legs_(legs), payer_(payer), currency_(currency), exercise_(exercise), settlementType_(settlementType),
      settlementMethod_(settlementMethod) {

    QL_REQUIRE(legs_.size() > 0, "MultiLegOption: No legs are given");
    QL_REQUIRE(payer_.size() == legs_.size(),
               "MultiLegOption: payer size (" << payer.size() << ") does not match legs size (" << legs_.size() << ")");
    QL_REQUIRE(currency_.size() == legs_.size(), "MultiLegOption: currency size (" << currency_.size()
                                                                                   << ") does not match legs size ("
                                                                                   << legs_.size() << ")");
    // find maturity date

    maturity_ = Date::minDate();
    for (auto const& l : legs_) {
        if (!l.empty())
            maturity_ = std::max(maturity_, l.back()->date());
    }

    // register with underlying cashflows

    for (auto const& l : legs_) {
        for (auto const& c : l) {
            registerWith(c);
            if (auto lazy = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c))
                lazy->alwaysForwardNotifications();
        }
    }

} // MultiLegOption

void MultiLegOption::deepUpdate() {
    for (auto& l : legs_) {
        for (auto& c : l) {
            if (auto lazy = QuantLib::ext::dynamic_pointer_cast<LazyObject>(c))
                lazy->deepUpdate();
        }
    }
    update();
}

bool MultiLegOption::isExpired() const {
    Date today = Settings::instance().evaluationDate();
    if (exercise_ == nullptr || exercise_->dates().empty()) {
        return today >= maturityDate();
    } else {
        // only the option itself is represented, not something we exercised into
        return today >= exercise_->dates().back();
    }
}

Real MultiLegOption::underlyingNpv() const {
    calculate();
    QL_REQUIRE(underlyingNpv_ != Null<Real>(), "MultiLegOption: underlying npv not available");
    return underlyingNpv_;
}

void MultiLegOption::setupArguments(PricingEngine::arguments* args) const {
    MultiLegOption::arguments* tmp = dynamic_cast<MultiLegOption::arguments*>(args);
    QL_REQUIRE(tmp != nullptr, "MultiLegOption: wrong pricing engine argument type");
    tmp->legs = legs_;
    tmp->payer = payer_;
    tmp->currency = currency_;
    tmp->exercise = exercise_;
    tmp->settlementType = settlementType_;
    tmp->settlementMethod = settlementMethod_;
}

void MultiLegOption::fetchResults(const PricingEngine::results* r) const {
    Instrument::fetchResults(r);
    const MultiLegOption::results* results = dynamic_cast<const MultiLegOption::results*>(r);
    if (results) {
        underlyingNpv_ = results->underlyingNpv;
    } else {
        underlyingNpv_ = Null<Real>();
    }
}

void MultiLegOption::results::reset() {
    Instrument::results::reset();
    underlyingNpv = Null<Real>();
}

} // namespace QuantExt
