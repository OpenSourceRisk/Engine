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

#include <qle/pricingengines/midpointcdsenginemultistate.hpp>

#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/instruments/claim.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>

using namespace QuantLib;

namespace QuantExt {

MidPointCdsEngineMultiState::MidPointCdsEngineMultiState(
    const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves,
    const std::vector<Handle<Quote>> recoveryRates, const Handle<YieldTermStructure>& discountCurve,
    const Size mainResultState, const boost::optional<bool> includeSettlementDateFlows)
    : MidPointCdsEngine(Handle<DefaultProbabilityTermStructure>(), 0.0, discountCurve, includeSettlementDateFlows),
      defaultCurves_(defaultCurves), recoveryRates_(recoveryRates), mainResultState_(mainResultState) {
    QL_REQUIRE(defaultCurves.size() == recoveryRates.size(), "MidPointCdsEngineMultiState: number of default curves ("
                                                                 << defaultCurves_.size()
                                                                 << ") must match number of recovery rates ("
                                                                 << recoveryRates_.size() << ")");
    QL_REQUIRE(!defaultCurves.empty(), "MidPointCdsEngineMultiState: no default curves / recovery rates given");
    for (auto const& h : defaultCurves_) {
        registerWith(h);
    }
    for (auto const& r : recoveryRates_) {
        registerWith(r);
    }
    QL_REQUIRE(mainResultState < defaultCurves.size(), "MidPointCdsEngineMultiState: mainResultState ("
                                                           << mainResultState << ") out of range 0..."
                                                           << defaultCurves.size() - 1);
}

void MidPointCdsEngineMultiState::linkCurves(Size i) const {
    probability_ = defaultCurves_[i];
    recoveryRate_ = recoveryRates_[i]->value();
}

void MidPointCdsEngineMultiState::calculate() const {

    // calculate all states except the main states

    std::vector<Real> values(defaultCurves_.size() + 1);
    for (Size i = 0; i < defaultCurves_.size(); ++i) {
        if (i == mainResultState_)
            continue;
        linkCurves(i);
        MidPointCdsEngine::calculate();
        values[i] = results_.value;
    }

    // calculate the main state last to keep the results from this calculation

    linkCurves(mainResultState_);
    MidPointCdsEngine::calculate();
    values[mainResultState_] = results_.value;

    // calculate the default state

    values.back() = calculateDefaultValue();

    // set additional result

    results_.additionalResults["stateNpv"] = values;
}

Real MidPointCdsEngineMultiState::calculateDefaultValue() const {
    Date defaultDate = discountCurve_->referenceDate();
    Real phi = arguments_.side == Protection::Seller ? -1.0 : 1.0;
    return phi * arguments_.claim->amount(defaultDate, arguments_.notional, recoveryRates_.back()->value());
}

} // namespace QuantExt
