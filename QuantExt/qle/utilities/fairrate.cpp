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

#include <qle/utilities/fairrate.hpp>

#include <cmath>

#include <ql/cashflows/cashflows.hpp>
#include <ql/cashflows/coupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>

using namespace QuantLib;

namespace QuantExt {

namespace {
const Spread basisPoint = 1.0e-4;

enum class LegKind { FixedLike, Floating, Other };

LegKind classifyLeg(const Leg& leg) {
    for (const auto& cf : leg) {
        if (auto frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf)) {
            return LegKind::Floating;
        }
        if (QuantLib::ext::dynamic_pointer_cast<Coupon>(cf)) {
            return LegKind::FixedLike;
        }
    }
    return LegKind::Other;
}

std::pair<Size, std::vector<Size>> selectReferenceLeg(const std::vector<Leg>& legs, const std::vector<bool>& isPayer) {
    QL_REQUIRE(!legs.empty(), "selectReferenceLeg: empty leg data");

    std::vector<Size> fixedIndices;
    Size firstFloatingIsPayer = Null<Size>();
    Size firstFloating = Null<Size>();

    for (Size i = 0; i < legs.size(); ++i) {
        LegKind kind = classifyLeg(legs[i]);
        if (kind == LegKind::FixedLike) {
            fixedIndices.push_back(i);
        } else if (kind == LegKind::Floating) {
            if(isPayer[i] && firstFloatingIsPayer == Null<Size>())
                firstFloatingIsPayer = i;
            if(firstFloating == Null<Size>())
                firstFloating = i;
        }
    }

    if (!fixedIndices.empty()) {
        Size ref = fixedIndices.front();
        std::vector<Size> exclude(fixedIndices.begin() + 1, fixedIndices.end());
        return {ref, exclude};
    }

    if (firstFloatingIsPayer != Null<Size>())
        return {firstFloatingIsPayer, {}};

    if (firstFloating != Null<Size>())
        return {firstFloating, {}};

    return {0, {}};
}

} // namespace

std::pair<Real, Real> fairRate(const std::vector<Leg>& legs,
                               const std::vector<bool>& isPayer,
                               const std::vector<Handle<YieldTermStructure>>& discountCurves,
                               const std::vector<Real>& fxSpotToBase) {

    // --- Input validation ---
    Size n = legs.size();
    QL_REQUIRE(n > 0, "fairRate: no legs provided");
    QL_REQUIRE(isPayer.size() == n, "fairRate: isPayer size mismatch");
    QL_REQUIRE(discountCurves.size() == 1 || discountCurves.size() == n, "fairRate: discountCurves size mismatch");
    QL_REQUIRE(fxSpotToBase.empty() || fxSpotToBase.size() == n, "fairRate: fxSpotToBase size mismatch");

    auto discountCurve = [&](Size i) -> const Handle<YieldTermStructure>& {
        return discountCurves.size() == 1 ? discountCurves.front() : discountCurves[i];
    };

    auto fxSpot = [&](Size i) -> Real { return fxSpotToBase.empty() ? 1.0 : fxSpotToBase[i]; };

    for (Size i = 0; i < n; ++i)
        QL_REQUIRE(!discountCurve(i).empty(), "fairRate: empty discount curve at leg " << i);

    Date valuationDate = discountCurve(0)->referenceDate();

    auto [referenceLegIdx, excludeIndices] = selectReferenceLeg(legs, isPayer);
    std::vector<bool> exclude(n, false);
    for (const auto i : excludeIndices)
        exclude[i] = true;

    const LegKind refKind = classifyLeg(legs[referenceLegIdx]);
    const bool refIsFixed = (refKind == LegKind::FixedLike);

    // Step 3 — Compute signed NPV in base ccy for all non-reference legs
    // after stripping floating spread contributions
    Real totalNpvNonRef = 0.0;
    Real spreadNpvNonRefBase = 0.0;

    for (Size l = 0; l < n; ++l) {
        if (l == referenceLegIdx || exclude[l])
            continue;

        auto npvAndBps = CashFlows::npvbps(legs[l], **discountCurve(l));
        Real npv = npvAndBps.first;

        Real spreadNpv = 0.0;
        const YieldTermStructure& dc = **discountCurve(l);
        for (const auto& cf : legs[l]) {
            if (cf->hasOccurred(valuationDate) || cf->tradingExCoupon(valuationDate))
                continue;
            if (auto frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf)) {
                spreadNpv += frc->nominal() * frc->accrualPeriod() * frc->spread() * dc.discount(cf->date());
            }
        }

        Real strippedNpv = npv - spreadNpv;
        Real dir = isPayer[l] ? -1.0 : 1.0;
        totalNpvNonRef += strippedNpv * dir * fxSpot(l);
        spreadNpvNonRefBase += spreadNpv * dir * fxSpot(l);
    }

    // --- Fixed reference leg ---
    if (refIsFixed) {
        auto npvAndBpsRef = CashFlows::npvbps(legs[referenceLegIdx], **discountCurve(referenceLegIdx));
        Real bpsRef = npvAndBpsRef.second;
        Real dirRef = isPayer[referenceLegIdx] ? -1.0 : 1.0;
        Real annuityBase = (bpsRef / basisPoint) * dirRef * fxSpot(referenceLegIdx);

        QL_REQUIRE(std::fabs(annuityBase) > QL_EPSILON, "fairRate: zero annuity on fixed reference leg");
        Real fair = -totalNpvNonRef / annuityBase;
        Real spreadCorrection = spreadNpvNonRefBase / annuityBase;
        return {fair, spreadCorrection};
    }

    // --- Floating reference leg ---
    const auto& refLeg = legs[referenceLegIdx];
    const YieldTermStructure& dc = **discountCurve(referenceLegIdx);

    Real strippedNpv = 0.0;
    Real annuityRaw = 0.0;

    for (const auto& cf : refLeg) {
        if (cf->hasOccurred(valuationDate) || cf->tradingExCoupon(valuationDate))
            continue;

        Real df = dc.discount(cf->date());

        if (auto frc = QuantLib::ext::dynamic_pointer_cast<FloatingRateCoupon>(cf)) {
            QL_REQUIRE(frc->pricer(), "fairRate: floating reference coupon without pricer");
            Real effectiveRate = frc->rate() - frc->spread();

            Real amt = frc->nominal() * frc->accrualPeriod() * effectiveRate;
            strippedNpv += amt * df;
            annuityRaw += frc->nominal() * frc->accrualPeriod() * df;
        } else if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(cf)) {
            strippedNpv += cpn->amount() * df;
            annuityRaw += cpn->nominal() * cpn->accrualPeriod() * df;
        } else {
            strippedNpv += cf->amount() * df;
        }
    }

    Real dirRef = isPayer[referenceLegIdx] ? -1.0 : 1.0;
    Real fxRef = fxSpot(referenceLegIdx);
    Real strippedBase = strippedNpv * dirRef * fxRef;
    Real annuityBase = annuityRaw * dirRef * fxRef;

    Real totalNpv = totalNpvNonRef + strippedBase;

    QL_REQUIRE(std::fabs(annuityBase) > QL_EPSILON, "fairRate: zero annuity on floating reference leg");
    Real fair = -totalNpv / annuityBase;
    Real spreadCorrection = spreadNpvNonRefBase / annuityBase;
    return {fair, spreadCorrection};
}

std::pair<Real, Real> fairRateFromNpvBps(Real fixedBps, Real floatingNpv, Real floatingSpreadNpv) {
    if (std::fabs(fixedBps) <= QL_EPSILON)
        return {0.0, 0.0};

    Real fairForward = -(floatingNpv - floatingSpreadNpv) / fixedBps;
    Real spreadCorrection = floatingSpreadNpv / fixedBps;
    return {fairForward, spreadCorrection};
}

} // namespace QuantExt
