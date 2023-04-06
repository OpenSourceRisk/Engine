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
    const std::vector<Handle<DefaultProbabilityTermStructure>>& defaultCurves, const Size mainResultState,
    const std::vector<Handle<Quote>>& recoveryRates, const Handle<Quote>& securitySpread,
    boost::optional<bool> includeSettlementDateFlows)
    : discountCurve_(boost::make_shared<ZeroSpreadedTermStructure>(discountCurve, securitySpread)),
      defaultCurves_(defaultCurves), mainResultState_(mainResultState), recoveryRates_(recoveryRates),
      securitySpread_(securitySpread), includeSettlementDateFlows_(includeSettlementDateFlows) {
    registerWith(discountCurve_);
    for (auto const& h : defaultCurves_) {
        QL_REQUIRE(!h.empty(), "DiscountingRiskyBondEngineMultiState: empty default curve handle");
        registerWith(h);
    }
    for (auto const& r : recoveryRates_) {
        QL_REQUIRE(!r.empty(), "DiscountingRiskyBondEngineMultiState: empty recovery handle");
        registerWith(r);
    }
    registerWith(securitySpread_);
    QL_REQUIRE(mainResultState_ < defaultCurves_.size(),
               "main result state (" << mainResultState_ << ") inconsistent with default curves size "
                                     << defaultCurves_.size());
}

void DiscountingRiskyBondEngineMultiState::calculate() const {
    // FIXME this is bascially a copy of DiscountingRiskyBondEngine, factor out the common computation
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    results_.valuationDate = discountCurve_->referenceDate();
    std::vector<Real> values(defaultCurves_.size() + 1);
    for (Size i = 0; i < defaultCurves_.size(); ++i) {
        values[i] = calculateNpv(i);
    }
    // FIXME settlement value is set to value here for simplicity
    results_.value = results_.settlementValue = values[mainResultState_];
    // calculate default state
    values.back() = calculateDefaultValue();
    results_.additionalResults["stateNpv"] = values;
}

Real DiscountingRiskyBondEngineMultiState::calculateNpv(const Size state) const {
    Real npvValue = 0.0;
    Date npvDate = discountCurve_->referenceDate();

    for (auto& cf : arguments_.cashflows) {
        if (cf->hasOccurred(npvDate, includeSettlementDateFlows_))
            continue;

        Probability S = defaultCurves_[state]->survivalProbability(cf->date());
        npvValue += cf->amount() * S * discountCurve_->discount(cf->date());

        // Add recovery value contributions using the coupon schedule dates as time discretisation
        // and coupon period mid points as default dates (consistent with CDS mid point engine).
        // FIXME: This works for fixed and floating coupon bonds, does not for zero bonds.
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            Date startDate = coupon->accrualStartDate();
            Date endDate = coupon->accrualEndDate();
            Date effectiveStartDate = (startDate <= npvDate && npvDate <= endDate) ? npvDate : startDate;
            Date defaultDate = effectiveStartDate + (endDate - effectiveStartDate) / 2;
            Probability P = defaultCurves_[state]->defaultProbability(effectiveStartDate, endDate);
            npvValue += coupon->nominal() * recoveryRates_[state]->value() * P * discountCurve_->discount(defaultDate);
        }
    }
    return npvValue;
}

Real DiscountingRiskyBondEngineMultiState::calculateDefaultValue() const {
    Date npvDate = discountCurve_->referenceDate();
    for (auto& cf : arguments_.cashflows) {
        if (cf->hasOccurred(npvDate, includeSettlementDateFlows_))
            continue;
        boost::shared_ptr<Coupon> coupon = boost::dynamic_pointer_cast<Coupon>(cf);
        if (coupon) {
            return coupon->nominal() * recoveryRates_.back()->value();
        }
    }
    // FIXME handle bonds without coupons
    QL_FAIL("could not calculated default value, no alive coupons found");
}

} // namespace QuantExt
