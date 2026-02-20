/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <qle/instruments/cashflowresults.hpp>
#include <qle/pricingengines/fdblackscholesvanillaengine.hpp>

#include <ql/exercise.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/termstructures/yieldtermstructure.hpp>
#include <ql/compounding.hpp>

namespace QuantExt {

void FdBlackScholesVanillaEngine2::calculate() const {

    // do the calculation in the base engine

    QuantLib::FdBlackScholesVanillaEngine::calculate();

    // add expected flow

    QuantLib::Date lastDate = arguments_.exercise->lastDate();

    std::vector<QuantExt::CashFlowResults> cfResults;

    cfResults.emplace_back();
    cfResults.back().amount = results_.value / process_->riskFreeRate()->discount(lastDate);
    cfResults.back().payDate = lastDate;
    cfResults.back().legNumber = 0;
    cfResults.back().type = "ExpectedFlow";
    auto payoff = dynamic_pointer_cast<QuantLib::StrikedTypePayoff>(arguments_.payoff);
    results_.additionalResults["cashFlowResults"] = cfResults;
    auto rf = process_->riskFreeRate()->discount(lastDate);
    auto dividendYield = process_->dividendYield()->discount(lastDate);
    results_.additionalResults["riskFreeDiscount"] = rf;
    results_.additionalResults["dividendDiscount"] = dividendYield;
    results_.additionalResults["volatility"] = process_->blackVolatility()->blackVol(
        lastDate, payoff->strike());
    results_.additionalResults["spot"] = process_->x0();
    auto tte = process_->blackVolatility()->timeFromReference(lastDate);
    results_.additionalResults["strike"] = payoff->strike();
    results_.additionalResults["timeToExpiry"] = tte;
    results_.additionalResults["forward"] = process_->x0() * dividendYield / rf;
}

} // namespace QuantExt
