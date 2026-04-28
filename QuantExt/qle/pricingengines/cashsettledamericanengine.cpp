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
#include <qle/pricingengines/cashsettledamericanengine.hpp>

using QuantLib::Date;
using QuantLib::DiscountFactor;
using QuantLib::Handle;
using QuantLib::Null;
using QuantLib::Real;
using QuantLib::Settings;
using QuantLib::VanillaOption;
using QuantLib::YieldTermStructure;


namespace QuantExt {

CashSettledAmericanEngine::CashSettledAmericanEngine(
    const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& underlyingEngine,
    const QuantLib::Handle<QuantLib::YieldTermStructure>& discountCurve)
    : underlyingEngine_(underlyingEngine), discountCurve_(discountCurve) {
    registerWith(discountCurve_);
}

void CashSettledAmericanEngine::calculate() const {

    QL_REQUIRE(discountCurve_.currentLink(), "CashSettledAmericanEngine: discount curve is empty.");

    const auto& dts = discountCurve_.currentLink();
    Date expiryDate = arguments_.exercise->lastDate();
    Date today = Settings::instance().evaluationDate();

    if (expiryDate <= today) {
        // Option has expired - determine payoff from exercise or automatic exercise.
        Real payoffAmount = 0.0;
        if (arguments_.exercised) {
            QL_REQUIRE(arguments_.priceAtExercise != Null<Real>(),
                       "Expect a valid price at exercise when option has been manually exercised.");
            payoffAmount = (*arguments_.payoff)(arguments_.priceAtExercise);
        } else if (arguments_.automaticExercise) {
            QL_REQUIRE(arguments_.underlying, "Expect a valid underlying index when exercise is automatic.");
            Real fixing = arguments_.underlying->fixing(expiryDate);
            payoffAmount = (*arguments_.payoff)(fixing);
        }

        Real fxRate = 1.0;
        if (arguments_.fxIndex != nullptr) {
            Date fixingDate = arguments_.cashSettlementFxFixingDate.has_value()
                                  ? *arguments_.cashSettlementFxFixingDate
                                  : arguments_.fxIndex->fixingDate(arguments_.paymentDate);
            fxRate = arguments_.fxIndex->fixing(fixingDate, false);
        }

        DiscountFactor df_tp = dts->discount(arguments_.paymentDate);
        results_.value = df_tp * payoffAmount * fxRate;
        results_.additionalResults["settlementFxFwd"] = fxRate;

    } else {
        // Option has not expired - delegate to the underlying American engine.

        // Build a temporary VanillaOption and price it with the underlying engine.
        auto tmpOption = QuantLib::ext::make_shared<VanillaOption>(
            QuantLib::ext::dynamic_pointer_cast<QuantLib::StrikedTypePayoff>(arguments_.payoff), arguments_.exercise);
        tmpOption->setPricingEngine(underlyingEngine_);
        Real underlyingNpv = tmpOption->NPV();

        // Deferred payment: discount from expiry to payment date.
        DiscountFactor df_te = dts->discount(expiryDate);
        DiscountFactor df_te_tp = dts->discount(arguments_.paymentDate) / df_te;

        // FX conversion.
        Real fxRate = 1.0;
        Date fixingDate;
        if (arguments_.fxIndex != nullptr) {
            fixingDate = arguments_.cashSettlementFxFixingDate.has_value()
                             ? *arguments_.cashSettlementFxFixingDate
                             : arguments_.fxIndex->fixingDate(arguments_.paymentDate);
            fxRate = arguments_.fxIndex->fixing(fixingDate, false);
        }

        results_.value = df_te_tp * underlyingNpv * fxRate;

        // Propagate additional results from the underlying engine.
        results_.additionalResults = tmpOption->additionalResults();
        results_.additionalResults["discountFactorTeTp"] = df_te_tp;
        results_.additionalResults["settlementFxFwd"] = fxRate;
    }
}

} // namespace QuantExt
