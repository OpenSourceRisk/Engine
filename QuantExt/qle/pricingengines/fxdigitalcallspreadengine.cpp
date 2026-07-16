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
        // If expiry has occurred, we attempt to establish the payoff amount, if any, and discount it.
        Real payoffAmount = 0.0;
        Real priceAtExercise = 0.0;
        bool deterministicPayoff = true;
        if (arguments_.automaticExercise) {
            // If we have automatic exercise, we base the payoff on the value of the index on the expiry date.
            QL_REQUIRE(arguments_.underlying, "Expect a valid underlying index when exercise is automatic.");
            priceAtExercise = arguments_.underlying->fixing(expiryDate);
            payoffAmount = (*arguments_.payoff)(priceAtExercise);
        } else if (arguments_.exercised) {
            // If we have manually exercised, we base the payoff on the value at exercise.
            QL_REQUIRE(arguments_.priceAtExercise != Null<Real>(), "Expect a valid price at exercise when option "
                                                                       << "has been manually exercised.");
            priceAtExercise = arguments_.priceAtExercise;
            payoffAmount = (*arguments_.payoff)(priceAtExercise);
        } else if (expiryDate == today) {
            // Expiry date is today, not automatic exercise and hasn't been manually exercised - use spot.
            priceAtExercise = bsp_->x0();
            payoffAmount = (*arguments_.payoff)(priceAtExercise);
            deterministicPayoff = false;
        }

        if(deterministicPayoff) {
            results_.delta = 0.0;
            results_.deltaForward = 0.0;
            results_.elasticity = 0.0;
            results_.gamma = 0.0;
            results_.dividendRho = 0.0;
            results_.vega = 0.0;
        }

        // Discount factor to payment date.
        DiscountFactor df_tp = dts->discount(arguments_.paymentDate);
        Time delta_tp = dts->timeFromReference(arguments_.paymentDate);

        // Only value, rho and theta are meaningful now.
        results_.value = df_tp * payoffAmount * fxRate;
        results_.rho = -delta_tp * results_.value;
        results_.theta = 0.0;
        if (delta_tp > 0.0 && !close(delta_tp, 0.0)) {
            results_.theta = -std::log(df_tp) / delta_tp * results_.value;
        }
        results_.thetaPerDay = results_.theta / 365.0;

        // Populate some additional results.
        results_.additionalResults["spot"] = bsp_->x0();
        auto payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
        if (payoff)
            results_.additionalResults["strike"] = payoff->strike();
        results_.additionalResults["priceAtExercise"] = priceAtExercise;
        results_.additionalResults["payoffAmount"] = payoffAmount;
        results_.additionalResults["discountFactor"] = df_tp;
        results_.additionalResults["timeToExpiry"] = delta_tp;
        results_.additionalResults["settlementFxFwd"] = fxRate;
    }else{

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

        auto lo = vanillaData(strikeLo);
        auto hi = vanillaData(strikeHi);
        Real priceLo = lo.first, deltaLo = lo.second;
        Real priceHi = hi.first, deltaHi = hi.second;

        Real cpSpread = cash * (priceLo - priceHi) / eps_;
        Real spreadValue = type == Option::Call ? cpSpread : -cpSpread;

        // Forward discount factor between expiry and payment date, P(t_e, t_p), under deterministic rates.
        DiscountFactor df_te_tp = dts->discount(arguments_.paymentDate) / dts->discount(expiryDate);

        Real spreadDelta = cash * (deltaLo - deltaHi) / eps_;
        if (type == Option::Put)
            spreadDelta = -spreadDelta;

        results_.value = df_te_tp * spreadValue * fxRate;
        results_.delta = df_te_tp * spreadDelta * fxRate;

        Real spot = bsp_->x0();
        DiscountFactor dividendDiscount = bsp_->dividendYield()->discount(expiryDate);
        DiscountFactor riskFreeDiscount = bsp_->riskFreeRate()->discount(expiryDate);

        results_.additionalResults["spot"] = spot;
        results_.additionalResults["forward"] = spot * dividendDiscount / riskFreeDiscount;
        results_.additionalResults["strike"] = strike;
        results_.additionalResults["dividendDiscount"] = dividendDiscount;
        results_.additionalResults["riskFreeDiscount"] = riskFreeDiscount;
        results_.additionalResults["settlementFxFwd"] = fxRate;

        results_.additionalResults["discountFactorTeTp"] = df_te_tp;
        results_.additionalResults["priceLo"] = priceLo;
        results_.additionalResults["priceHi"] = priceHi;
        results_.additionalResults["deltaLo"] = deltaLo;
        results_.additionalResults["deltaHi"] = deltaHi;
    }

    if (flipResults_) {

        // Invert strike, spot, forward

        auto resToInvert = std::vector<std::string>({"spot", "forward", "strike"});
        for (const std::string& res : resToInvert) {
            auto it = results_.additionalResults.find(res);
            if (it != results_.additionalResults.end()) {
                std::string resPricing = res + "_pricing";
                results_.additionalResults[resPricing] = it->second;
                it->second = 1. / QuantLib::ext::any_cast<Real>(it->second);
            }
        }

        // Swap riskFreeDiscount and dividendDiscount, discountFactor stays what it is

        Real rfDiscount = Null<Real>();
        Real divDiscount = Null<Real>();

        if (auto tmp = results_.additionalResults.find("riskFreeDiscount"); tmp != results_.additionalResults.end())
            rfDiscount = QuantLib::ext::any_cast<Real>(tmp->second);
        if (auto tmp = results_.additionalResults.find("dividendDiscount"); tmp != results_.additionalResults.end())
            divDiscount = QuantLib::ext::any_cast<Real>(tmp->second);

        results_.additionalResults["riskFreeDiscount"] = divDiscount;
        results_.additionalResults["dividendDiscount"] = rfDiscount;
    }

    
}

} // namespace QuantExt
