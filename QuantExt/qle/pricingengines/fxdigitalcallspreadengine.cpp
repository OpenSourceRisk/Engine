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

#include <qle/pricingengines/fxdigitalcallspreadengine.hpp>

#include <qle/pricingengines/analyticeuropeanengine.hpp>

#include <ql/exercise.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/instruments/vanillaoption.hpp>
#include <ql/settings.hpp>

namespace QuantExt {

using namespace QuantLib;

FxDigitalCallSpreadEngine::FxDigitalCallSpreadEngine(
    const QuantLib::ext::shared_ptr<QuantLib::GeneralizedBlackScholesProcess>& bsp, const bool flipResults,
    QuantLib::Real eps)
    : bsp_(bsp), flipResults_(flipResults), eps_(eps) {
    registerWith(bsp_);
}

void FxDigitalCallSpreadEngine::calculate() const {

    auto payoff = ext::dynamic_pointer_cast<CashOrNothingPayoff>(arguments_.payoff); // N x D
    QL_REQUIRE(payoff, "FxDigitalCallSpreadEngine requires a CashOrNothingPayoff");

    Real strike = payoff->strike();
    Real cash = payoff->cashPayoff();
    Option::Type type = payoff->optionType();

    Date expiryDate = arguments_.exercise->lastDate();
    Date today = Settings::instance().evaluationDate();

    auto dts = bsp_->riskFreeRate().currentLink();

    Real fxRate = 1.0;
    if (arguments_.fxIndex != nullptr) {
        Date fixingDate = arguments_.cashSettlementFxFixingDate.has_value()
                              ? *arguments_.cashSettlementFxFixingDate
                              : arguments_.fxIndex->fixingDate(expiryDate);
        fxRate = arguments_.fxIndex->fixing(fixingDate, false);
    }

    if (expiryDate <= today) {
        // Expiry has passed: the payout is fixed. Handled identically to
        // AnalyticCashSettledEuropeanEngine, discounting the cash payoff from the payment date.
        Real payoffAmount = 0.0;
        if (arguments_.automaticExercise) {
            QL_REQUIRE(arguments_.underlying, "Expect a valid underlying index when exercise is automatic.");
            payoffAmount = (*arguments_.payoff)(arguments_.underlying->fixing(expiryDate));
        } else if (arguments_.exercised) {
            QL_REQUIRE(arguments_.priceAtExercise != Null<Real>(),
                       "Expect a valid price at exercise when option has been manually exercised.");
            payoffAmount = (*arguments_.payoff)(arguments_.priceAtExercise);
        } else if (expiryDate == today) {
            payoffAmount = (*arguments_.payoff)(bsp_->x0());
        }

        DiscountFactor df_tp = dts->discount(arguments_.paymentDate);
        results_.value = df_tp * payoffAmount * fxRate;
        return;
    }

    // Expiry in the future: price the call spread assuming payment on the option expiry date, then
    // account for the delayed payment with the forward discount factor between expiry and payment.
    Real strikeLo = strike - eps_ / 2.0;
    Real strikeHi = strike + eps_ / 2.0;

    auto vanillaEngine = ext::make_shared<QuantExt::AnalyticEuropeanEngine>(bsp_, flipResults_);

    auto vanillaData = [&](Real k) {
        VanillaOption option(ext::make_shared<PlainVanillaPayoff>(type, k), arguments_.exercise);
        option.setPricingEngine(vanillaEngine);
        return std::make_pair(option.NPV(), option.delta());
    };

    Real priceLo = vanillaData(strikeLo).first;
    Real deltaLo = vanillaData(strikeLo).second;
    Real priceHi = vanillaData(strikeHi).first;
    Real deltaHi = vanillaData(strikeHi).second;

    Real spreadValue;
    if (type == Option::Call)
        spreadValue = cash * (priceLo - priceHi) / eps_;
    else
        spreadValue = cash * (priceHi - priceLo) / eps_;

    // Forward discount factor between expiry and payment date, P(t_e, t_p), under deterministic rates.
    DiscountFactor df_te_tp = dts->discount(arguments_.paymentDate) / dts->discount(expiryDate);

    results_.value = df_te_tp * spreadValue * fxRate;
    results_.delta = df_te_tp * (deltaHi - deltaLo);

    results_.additionalResults["discountFactorTeTp"] = df_te_tp;
    results_.additionalResults["settlementFxFwd"] = fxRate;
    results_.additionalResults["priceLo"] = priceLo;
    results_.additionalResults["priceHi"] = priceHi;
    results_.additionalResults["deltaLo"] = deltaLo;
    results_.additionalResults["deltaHi"] = deltaHi;
}

} // namespace QuantExt