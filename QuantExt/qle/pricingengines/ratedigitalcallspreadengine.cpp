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

#include <qle/pricingengines/ratedigitalcallspreadengine.hpp>

#include <ql/instruments/payoffs.hpp>
#include <ql/pricingengines/blackformula.hpp>

#include <cmath>

namespace QuantExt {

using namespace QuantLib;

RateDigitalCallSpreadEngine::RateDigitalCallSpreadEngine(Real forward,
                                                         DiscountFactor dfPayment,
                                                         Handle<OptionletVolatilityStructure> ovs,
                                                         Date fixingDate,
                                                         Real eps)
    : forward_(forward), df_(dfPayment), ovs_(std::move(ovs)),
      fixingDate_(fixingDate), eps_(eps) {
    registerWith(ovs_);
}

void RateDigitalCallSpreadEngine::calculate() const {
    auto payoff = ext::dynamic_pointer_cast<CashOrNothingPayoff>(arguments_.payoff);
    QL_REQUIRE(payoff, "RateDigitalCallSpreadEngine requires a CashOrNothingPayoff");

    const Real K = payoff->strike();
    const Real cash = payoff->cashPayoff();
    const Option::Type type = payoff->optionType();

    const Real K_lo = K - eps_ / 2.0;
    const Real K_hi = K + eps_ / 2.0;

    Real ttExpiry = ovs_->timeFromReference(fixingDate_);
    QL_REQUIRE(ttExpiry > 0.0, "RateDigitalCallSpreadEngine: fixing date "
                                   << fixingDate_ << " is not in the future");

    Real price;
    if (ovs_->volatilityType() == Normal) {
        // Bachelier (normal) formula
        Real volLo = ovs_->volatility(fixingDate_, K_lo);
        Real volHi = ovs_->volatility(fixingDate_, K_hi);
        Real stdLo = volLo * std::sqrt(ttExpiry);
        Real stdHi = volHi * std::sqrt(ttExpiry);

        if (type == Option::Call) {
            Real cLo = bachelierBlackFormula(Option::Call, K_lo, forward_, stdLo, df_);
            Real cHi = bachelierBlackFormula(Option::Call, K_hi, forward_, stdHi, df_);
            price = cash * (cLo - cHi) / eps_;
        } else {
            Real pLo = bachelierBlackFormula(Option::Put, K_lo, forward_, stdLo, df_);
            Real pHi = bachelierBlackFormula(Option::Put, K_hi, forward_, stdHi, df_);
            price = cash * (pHi - pLo) / eps_;
        }
    } else {
        // Black (shifted-lognormal) formula
        Real displacement = ovs_->displacement();
        Real volLo = ovs_->volatility(fixingDate_, K_lo);
        Real volHi = ovs_->volatility(fixingDate_, K_hi);
        Real stdLo = volLo * std::sqrt(ttExpiry);
        Real stdHi = volHi * std::sqrt(ttExpiry);

        if (type == Option::Call) {
            Real cLo = blackFormula(Option::Call, K_lo, forward_, stdLo, df_, displacement);
            Real cHi = blackFormula(Option::Call, K_hi, forward_, stdHi, df_, displacement);
            price = cash * (cLo - cHi) / eps_;
        } else {
            Real pLo = blackFormula(Option::Put, K_lo, forward_, stdLo, df_, displacement);
            Real pHi = blackFormula(Option::Put, K_hi, forward_, stdHi, df_, displacement);
            price = cash * (pHi - pLo) / eps_;
        }
    }

    results_.value = price;
}

} // namespace QuantExt
