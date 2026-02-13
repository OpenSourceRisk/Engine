/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <ored/utilities/fairrate.hpp>
#include <ored/utilities/log.hpp>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/settings.hpp>

using namespace QuantLib;

namespace ore {
namespace data {

namespace {
    const Spread basisPoint = 1.0e-4;
}

// Step 1 — Determine the Reference Leg
std::pair<Size, std::vector<Size>>
selectReferenceLeg(const std::vector<LegData>& legData) {

    // Fixed-leg types (from isdaSubProductSwap classification)
    auto isFixed = [](LegType t) {
        return t == LegType::Fixed || t == LegType::ZeroCouponFixed
            || t == LegType::Cashflow || t == LegType::CommodityFixed;
    };

    // Rule 1 & 2: find all fixed legs
    std::vector<Size> fixedIndices;
    for (Size i = 0; i < legData.size(); ++i) {
        if (isFixed(legData[i].legType()))
            fixedIndices.push_back(i);
    }

    if (!fixedIndices.empty()) {
        Size ref = fixedIndices.front();
        std::vector<Size> exclude(fixedIndices.begin() + 1,
                                  fixedIndices.end());
        return {ref, exclude};
    }

    // Rule 3: no fixed leg — find first floating with spread
    for (Size i = 0; i < legData.size(); ++i) {
        if (legData[i].legType() == LegType::Floating) {
            auto fld = QuantLib::ext::dynamic_pointer_cast<FloatingLegData>(
                legData[i].concreteLegData());
            if (fld && !fld->spreads().empty()) {
                bool hasNonZeroSpread = false;
                for (auto s : fld->spreads()) {
                    if (std::fabs(s) > QL_EPSILON) {
                        hasNonZeroSpread = true;
                        break;
                    }
                }
                if (hasNonZeroSpread)
                    return {i, {}};
            }
        }
    }

    // Fallback: first floating leg (even without a spread)
    for (Size i = 0; i < legData.size(); ++i) {
        if (legData[i].legType() == LegType::Floating)
            return {i, {}};
    }

    // Final fallback: first leg
    QL_REQUIRE(!legData.empty(), "selectReferenceLeg: empty leg data");
    return {0, {}};
}

Real fairRate(const std::vector<Leg>& legs,
              const std::vector<bool>& isPayer,
              const std::vector<Handle<YieldTermStructure>>& discountCurves,
              const std::vector<Real>& fxSpotToBase,
              Size referenceLegIdx) {

    // --- Input validation ---
    Size n = legs.size();
    QL_REQUIRE(n > 0, "fairRate: no legs provided");
    QL_REQUIRE(isPayer.size() == n, "fairRate: isPayer size mismatch");
    QL_REQUIRE(discountCurves.size() == n,
               "fairRate: discountCurves size mismatch");
    QL_REQUIRE(fxSpotToBase.size() == n,
               "fairRate: fxSpotToBase size mismatch");
    QL_REQUIRE(referenceLegIdx < n,
               "fairRate: referenceLegIdx " << referenceLegIdx
               << " out of range [0," << n << ")");

    // Step 2 — Classify reference leg
    bool refIsFixed = true;
    for (const auto& cf : legs[referenceLegIdx]) {
        if (QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf)) {
            refIsFixed = false;
            break;
        }
    }

    // Step 3 — Compute signed NPV in base ccy for all non-reference legs
    Real totalNpvNonRef = 0.0;
    for (Size l = 0; l < n; ++l) {
        if (l == referenceLegIdx)
            continue;
        auto [npv, bps] = CashFlows::npvbps(
            legs[l], *discountCurves[l].operator->());
        Real dir = isPayer[l] ? -1.0 : 1.0;
        totalNpvNonRef += npv * dir * fxSpotToBase[l];
    }

    // --- Fixed reference leg ---
    if (refIsFixed) {
        auto [npvRef, bpsRef] = CashFlows::npvbps(
            legs[referenceLegIdx],
            *discountCurves[referenceLegIdx].operator->());
        Real dirRef = isPayer[referenceLegIdx] ? -1.0 : 1.0;
        Real annuityBase = (bpsRef / basisPoint)
                         * dirRef * fxSpotToBase[referenceLegIdx];

        QL_REQUIRE(std::fabs(annuityBase) > QL_EPSILON,
                   "fairRate: zero annuity on fixed reference leg");
        return -totalNpvNonRef / annuityBase;
    }

    // --- Floating reference leg ---
    const auto& refLeg = legs[referenceLegIdx];
    const YieldTermStructure& dc = *discountCurves[referenceLegIdx].operator->();
    Date settl = Settings::instance().evaluationDate();

    Real strippedNpv = 0.0;
    Real annuityRaw  = 0.0;

    for (const auto& cf : refLeg) {
        if (cf->hasOccurred(settl) || cf->tradingExCoupon(settl))
            continue;

        Real df = dc.discount(cf->date());

        if (auto frc = QuantLib::ext::dynamic_pointer_cast<
                FloatingRateCoupon>(cf)) {
            // Use the pricer-computed rate if available (includes adjustments),
            // otherwise fall back to raw index fixing
            Real effectiveRate;
            
            if (frc->pricer()) {
                try {
                    // rate() returns: gearing * adjustedFixing + spread
                    Real pricerRate = frc->rate();
                    Real spread = frc->spread();
                    // Strip spread to get: gearing * adjustedFixing
                    effectiveRate = pricerRate - spread;
                } catch (...) {
                    // Pricer not computed yet, use raw fixing
                    effectiveRate = frc->gearing() * frc->indexFixing();
                }
            } else {
                // No pricer attached, use raw fixing
                effectiveRate = frc->gearing() * frc->indexFixing();
            }
            
            Real amt = frc->nominal() * frc->accrualPeriod() * effectiveRate;
            strippedNpv += amt * df;
            annuityRaw  += frc->nominal()
                         * frc->accrualPeriod() * df;
        } else if (auto cpn = QuantLib::ext::dynamic_pointer_cast<
                       Coupon>(cf)) {
            strippedNpv += cpn->amount() * df;
            annuityRaw  += cpn->nominal()
                         * cpn->accrualPeriod() * df;
        } else {
            strippedNpv += cf->amount() * df;
        }
    }

    DiscountFactor npvDateDf = dc.discount(settl);
    if (std::fabs(npvDateDf) > QL_EPSILON) {
        strippedNpv /= npvDateDf;
        annuityRaw  /= npvDateDf;
    }

    Real dirRef     = isPayer[referenceLegIdx] ? -1.0 : 1.0;
    Real fxRef      = fxSpotToBase[referenceLegIdx];
    Real strippedBase = strippedNpv * dirRef * fxRef;
    Real annuityBase  = annuityRaw  * dirRef * fxRef;

    Real totalNpv = totalNpvNonRef + strippedBase;

    QL_REQUIRE(std::fabs(annuityBase) > QL_EPSILON,
               "fairRate: zero annuity on floating reference leg");
    return -totalNpv / annuityBase;
}

} // namespace data
} // namespace ore
