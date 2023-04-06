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
    : discountCurve_(discountCurve), defaultCurves_(defaultCurves), mainResultState_(mainResultState),
      recoveryRates_(recoveryRates), includeSettlementDateFlows_(includeSettlementDateFlows) {
    QL_REQUIRE(!discountCurve_.empty(), "MidPointCdsEngineMultiState: no discount curve given");
    registerWith(discountCurve_);
    for (auto const& h : defaultCurves_) {
        QL_REQUIRE(!h.empty(), "MidPointCdsEngineMultiState: empty default curve handle");
        registerWith(h);
    }
    for (auto const& r : recoveryRates_) {
        QL_REQUIRE(!r.empty(), "MidPointCdsEngineMultiState: empty recovery handle");
        registerWith(r);
    }
    QL_REQUIRE(mainResultState_ < defaultCurves_.size(),
               "main result state (" << mainResultState_ << ") inconsistent with default curves size "
                                     << defaultCurves_.size());
}

void MidPointCdsEngineMultiState::calculate() const {
    // FIXME this is basically a copy of MidPointCdsEngine, factor out the common computation
    QL_REQUIRE(!discountCurve_.empty(), "discounting term structure handle is empty");
    results_.valuationDate = discountCurve_->referenceDate();
    std::vector<Real> values(defaultCurves_.size() + 1);
    for (Size i = 0; i < defaultCurves_.size(); ++i) {
        values[i] = calculateNpv(i);
    }
    results_.value = values[mainResultState_];
    // calculate default state
    values.back() = calculateDefaultValue();
    results_.additionalResults["stateNpv"] = values;
}

Real MidPointCdsEngineMultiState::calculateNpv(const Size state) const {
    Real npvValue = 0.0;
    Date today = Settings::instance().evaluationDate();
    Date settlementDate = discountCurve_->referenceDate();

    Real couponLegNPV = 0.0, defaultLegNPV = 0.0, upfrontNPV = 0.0;

    // Upfront Flow NPV. Either we are on-the-run (no flow)
    // or we are forward start
    Real upfPVO1 = 0.0;
    if (!arguments_.upfrontPayment->hasOccurred(settlementDate, includeSettlementDateFlows_)) {
        // date determining the probability survival so we have to pay
        //   the upfront (did not knock out)
        Date effectiveUpfrontDate = arguments_.protectionStart > defaultCurves_[state]->referenceDate()
                                        ? arguments_.protectionStart
                                        : defaultCurves_[state]->referenceDate();
        upfPVO1 = defaultCurves_[state]->survivalProbability(effectiveUpfrontDate) *
                  discountCurve_->discount(arguments_.upfrontPayment->date());
    }
    upfrontNPV = upfPVO1 * arguments_.upfrontPayment->amount();

    for (Size i = 0; i < arguments_.leg.size(); ++i) {
        if (arguments_.leg[i]->hasOccurred(settlementDate, includeSettlementDateFlows_))
            continue;

        boost::shared_ptr<FixedRateCoupon> coupon = boost::dynamic_pointer_cast<FixedRateCoupon>(arguments_.leg[i]);

        // In order to avoid a few switches, we calculate the NPV
        // of both legs as a positive quantity. We'll give them
        // the right sign at the end.

        Date paymentDate = coupon->date(), startDate = coupon->accrualStartDate(), endDate = coupon->accrualEndDate();
        // this is the only point where it might not coincide
        if (i == 0)
            startDate = arguments_.protectionStart;
        Date effectiveStartDate = (startDate <= today && today <= endDate) ? today : startDate;
        Date defaultDate = // mid-point
            effectiveStartDate + (endDate - effectiveStartDate) / 2;

        Probability S = defaultCurves_[state]->survivalProbability(paymentDate);
        Probability P = defaultCurves_[state]->defaultProbability(effectiveStartDate, endDate);

        // on one side, we add the fixed rate payments in case of
        // survival...
        couponLegNPV += S * coupon->amount() * discountCurve_->discount(paymentDate);
        // ...possibly including accrual in case of default.
        if (arguments_.settlesAccrual) {
            if (arguments_.paysAtDefaultTime) {
                couponLegNPV += P * coupon->accruedAmount(defaultDate) * discountCurve_->discount(defaultDate);
            } else {
                // pays at the end
                couponLegNPV += P * coupon->amount() * discountCurve_->discount(paymentDate);
            }
        }

        // on the other side, we add the payment in case of default.
        Real claim = arguments_.claim->amount(defaultDate, arguments_.notional, recoveryRates_[state]->value());
        if (arguments_.paysAtDefaultTime) {
            defaultLegNPV += P * claim * discountCurve_->discount(defaultDate);
        } else {
            defaultLegNPV += P * claim * discountCurve_->discount(paymentDate);
        }
    }

    // Real upfrontSign = 1.0;
    switch (arguments_.side) {
    case Protection::Seller:
        defaultLegNPV *= -1.0;
        break;
    case Protection::Buyer:
        couponLegNPV *= -1.0;
        upfrontNPV *= -1.0;
        // upfrontSign = -1.0;
        break;
    default:
        QL_FAIL("unknown protection side");
    }

    npvValue = defaultLegNPV + couponLegNPV + upfrontNPV;
    return npvValue;
} // calculateNpv

Real MidPointCdsEngineMultiState::calculateDefaultValue() const {
    Date defaultDate = discountCurve_->referenceDate();
    Real phi = arguments_.side == Protection::Seller ? -1.0 : 1.0;
    return phi * arguments_.claim->amount(defaultDate, arguments_.notional, recoveryRates_.back()->value());
}

} // namespace QuantExt
