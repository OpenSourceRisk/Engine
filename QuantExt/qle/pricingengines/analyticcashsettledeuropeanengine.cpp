/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ql/event.hpp>
#include <ql/exercise.hpp>
#include <qle/indexes/commodityindex.hpp>
#include <qle/pricingengines/analyticcashsettledeuropeanengine.hpp>

using QuantLib::close;
using QuantLib::Date;
using QuantLib::DiscountFactor;
using QuantLib::GeneralizedBlackScholesProcess;
using QuantLib::Handle;
using QuantLib::Null;
using QuantLib::PricingEngine;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::StrikedTypePayoff;
using QuantLib::Time;
using QuantLib::VanillaOption;
using QuantLib::YieldTermStructure;
using QuantLib::detail::simple_event;

namespace QuantExt {

AnalyticCashSettledEuropeanEngine::AnalyticCashSettledEuropeanEngine(
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& bsp)
    : underlyingEngine_(bsp), bsp_(bsp) {
    registerWith(bsp_);
}

AnalyticCashSettledEuropeanEngine::AnalyticCashSettledEuropeanEngine(
    const QuantLib::ext::shared_ptr<GeneralizedBlackScholesProcess>& bsp, const Handle<YieldTermStructure>& discountCurve)
    : underlyingEngine_(bsp, discountCurve), bsp_(bsp), discountCurve_(discountCurve) {
    registerWith(bsp_);
    registerWith(discountCurve_);
}

void AnalyticCashSettledEuropeanEngine::calculate() const {

    // Same logic as underlying engine for discount curve.
    QuantLib::ext::shared_ptr<YieldTermStructure> dts =
        discountCurve_.empty() ? bsp_->riskFreeRate().currentLink() : discountCurve_.currentLink();

    // Option expiry date.
    Date expiryDate = arguments_.exercise->lastDate();

    Date today = Settings::instance().evaluationDate();
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
        results_.value = df_tp * payoffAmount;
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

    } else {

        // If expiry has not occurred, we use the underlying engine and amend the results to account
        // for the deferred cash payment.

        // Prepare the underlying engine for the valuation.
        underlyingEngine_.reset();
        VanillaForwardOption::arguments* underlyingArgs =
            dynamic_cast<VanillaForwardOption::arguments*>(underlyingEngine_.getArguments());
        QL_REQUIRE(underlyingArgs, "Underlying engine expected to have vanilla option arguments.");
        underlyingArgs->exercise = arguments_.exercise;
        underlyingArgs->payoff = arguments_.payoff;

        // If we have a commodity future index, set the forward date to its expiry to get the right future price 
        // in the Black formula. If not, just use the expiry date => same result as standard QL engine.
        if (auto cfi = QuantLib::ext::dynamic_pointer_cast<CommodityFuturesIndex>(arguments_.underlying)) {
            underlyingArgs->forwardDate = cfi->expiryDate();
        } else {
            underlyingArgs->forwardDate = expiryDate;
        }
        underlyingEngine_.calculate();

        // Discount factor from payment date back to expiry date i.e. P(t_e, t_p) when rates are deterministic.
        DiscountFactor df_te_tp = dts->discount(arguments_.paymentDate) / dts->discount(expiryDate);
        Time delta_te_tp = dts->timeFromReference(arguments_.paymentDate) - dts->timeFromReference(expiryDate);

        // Populate this engine's results using the results from the underlying engine.
        const CashSettledEuropeanOption::results* underlyingResults =
            dynamic_cast<const CashSettledEuropeanOption::results*>(underlyingEngine_.getResults());
        QL_REQUIRE(underlyingResults, "Underlying engine expected to have compatible results.");

        results_.value = df_te_tp * underlyingResults->value;
        results_.delta = df_te_tp * underlyingResults->delta;
        results_.deltaForward = df_te_tp * underlyingResults->deltaForward;
        results_.elasticity = underlyingResults->elasticity;
        results_.gamma = df_te_tp * underlyingResults->gamma;
        results_.rho = df_te_tp * (underlyingResults->rho - delta_te_tp * underlyingResults->value);
        results_.dividendRho = df_te_tp * underlyingResults->dividendRho;
        results_.vega = df_te_tp * underlyingResults->vega;
        if (underlyingResults->theta != Null<Real>())
            results_.theta = df_te_tp * underlyingResults->theta;
        if (underlyingResults->thetaPerDay != Null<Real>())
            results_.thetaPerDay = df_te_tp * underlyingResults->thetaPerDay;
        results_.strikeSensitivity = df_te_tp * underlyingResults->strikeSensitivity;
        results_.itmCashProbability = underlyingResults->itmCashProbability;

        // Take the additional results from the underlying engine and add more.
        results_.additionalResults = underlyingResults->additionalResults;
        results_.additionalResults["discountFactorTeTp"] = df_te_tp;
    }

}

} // namespace QuantExt
