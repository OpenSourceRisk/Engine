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

#include <qle/pricingengines/discountingriskybondenginemultistate.hpp>

#include <ql/cashflows/coupon.hpp>
#include <ql/termstructures/yield/zerospreadedtermstructure.hpp>

#include <boost/make_shared.hpp>

using namespace QuantLib;

namespace QuantExt {

DiscountingRiskyBondEngineMultiState::DiscountingRiskyBondEngineMultiState(
    const Handle<YieldTermStructure>& discountCurve,
    const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves,
    const std::vector<Handle<Quote>>& recoveryRates, const Size mainResultState, const Handle<Quote>& securitySpread,
    Period timestepPeriod, boost::optional<bool> includeSettlementDateFlows)
    : DiscountingRiskyBondEngine(discountCurve, Handle<DefaultProbabilityTermStructure>(), Handle<Quote>(),
                                 securitySpread, timestepPeriod, includeSettlementDateFlows),
      defaultCurves_(defaultCurves), recoveryRates_(recoveryRates), mainResultState_(mainResultState) {
    QL_REQUIRE(!defaultCurves.empty(), "DiscountingRiskBondEngineMultiState: no default curves / recovery rates given");
    for (auto const& h : defaultCurves_) {
        registerWith(h);
    }
    for (auto const& r : recoveryRates_) {
        registerWith(r);
    }
    QL_REQUIRE(mainResultState < defaultCurves.size(), "DiscountingRiskBondEngineMultiState: mainResultState ("
                                                           << mainResultState << ") out of range 0..."
                                                           << defaultCurves.size() - 1);
}

void DiscountingRiskyBondEngineMultiState::linkCurves(Size i) const {
    defaultCurve_ = defaultCurves_[i];
    recoveryRate_ = recoveryRates_[i];
}

void DiscountingRiskyBondEngineMultiState::calculate() const {

    // calculate all states except the main states

    std::vector<Real> values(defaultCurves_.size() + 1);
    for (Size i = 0; i < defaultCurves_.size(); ++i) {
        if (i == mainResultState_)
            continue;
        linkCurves(i);
        DiscountingRiskyBondEngine::calculate();
        values[i] = results_.value;
    }

    // calculate the main state last to keep the results from this calculation

    linkCurves(mainResultState_);
    DiscountingRiskyBondEngine::calculate();
    values[mainResultState_] = results_.value;

    // calculate the default state

    values.back() = calculateDefaultValue();

    // set additional result

    results_.additionalResults["stateNpv"] = values;
}

Real DiscountingRiskyBondEngineMultiState::calculateDefaultValue() const {
    Date npvDate = discountCurve_->referenceDate();
    for (auto& cf : arguments_.cashflows) {
        if (cf->hasOccurred(npvDate, includeSettlementDateFlows_))
            continue;
        QuantLib::ext::shared_ptr<Coupon> coupon = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            return coupon->nominal() * recoveryRates_.back()->value();
        }
    }
    // FIXME handle bonds without coupons
    QL_FAIL("could not calculated default value, no alive coupons found");
}

} // namespace QuantExt
