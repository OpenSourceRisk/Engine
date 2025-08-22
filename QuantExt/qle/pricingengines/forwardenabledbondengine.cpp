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

#include <qle/pricingengines/forwardenabledbondengine.hpp>

namespace QuantExt {

std::pair<QuantLib::Real, QuantLib::Real>
forwardPrice(const QuantLib::ext::shared_ptr<QuantLib::Instrument>& instrument, const QuantLib::Date& forwardDate,
             const QuantLib::Date& settlementDate, const bool conditionalOnSurvival,
             std::vector<CashFlowResults>* cfResults) {
    instrument->pricingEngine()->reset();
    instrument->setupArguments(instrument->pricingEngine()->getArguments());
    instrument->pricingEngine()->getArguments()->validate();
    auto fwdEngine = QuantLib::ext::dynamic_pointer_cast<ForwardEnabledBondEngine>(instrument->pricingEngine());
    QL_REQUIRE(fwdEngine, "forwardPrice(): engine can not be cast to ForwardEnabledBondEngine");
    return fwdEngine->forwardPrice(forwardDate, settlementDate, conditionalOnSurvival, cfResults);
}

} // namespace QuantExt
