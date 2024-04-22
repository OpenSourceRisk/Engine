/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
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

#include <qle/models/crossassetanalytics.hpp>
#include <qle/pricingengines/commodityschwartzfutureoptionengine.hpp>
#include <ql/pricingengines/blackcalculator.hpp>

namespace QuantExt {

CommoditySchwartzFutureOptionEngine::CommoditySchwartzFutureOptionEngine(
    const QuantLib::ext::shared_ptr<CommoditySchwartzModel>& model) : model_(model) {}

void CommoditySchwartzFutureOptionEngine::calculate() const {

    QL_REQUIRE(arguments_.exercise->type() == Exercise::European, "only European options are allowed");

    QuantLib::ext::shared_ptr<StrikedTypePayoff> payoff = QuantLib::ext::dynamic_pointer_cast<StrikedTypePayoff>(arguments_.payoff);
    QL_REQUIRE(payoff != NULL, "only striked payoff is allowed");

    QuantLib::ext::shared_ptr<CommoditySchwartzParametrization> param = model_->parametrization();
    
    Date expiry = arguments_.exercise->lastDate();
    Time T = param->priceCurve()->timeFromReference(expiry);
    if (T <= 0.0) {
        // option is expired, we do not value any possibly non settled
        // flows, i.e. set the npv to zero in this case
        results_.value = 0.0;
        return;
    }
    Real forward = param->priceCurve()->price(expiry);
    // Var[ln F(T,T)] = Var[X(T)] = V(0,T) - V(T,T)
    Real variance = param->VtT(0,T) - param->VtT(T,T); 
    BlackCalculator black(payoff, forward, std::sqrt(variance), 1.0);
    results_.value = black.value();

} // calculate()

} // namespace QuantExt
