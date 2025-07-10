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

    results_.additionalResults["cashFlowResults"] = cfResults;
}

} // namespace QuantExt
