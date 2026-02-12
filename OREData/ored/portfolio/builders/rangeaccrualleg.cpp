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

#include <ored/portfolio/builders/rangeaccrualleg.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/indexparser.hpp>

#include <ql/cashflows/rangeaccrual.hpp>
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>

namespace ore {
namespace data {

Handle<OptionletVolatilityStructure> RangeAccrualLegEngineBuilder::optionletVolatilityStructure(const std::string& index) {
    bool zeroVolatility = parseBool(engineParameter("ZeroVolatility", {}, false, "false"));
    if (zeroVolatility) {
        return Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
            0, NullCalendar(), Unadjusted, 0.0, Actual365Fixed(), Normal));
    }
    auto configuration = this->configuration(MarketContext::pricing);
    return market_->capFloorVol(index, configuration);
}

Real RangeAccrualLegEngineBuilder::correlation(const std::string& index) {
    return parseReal(engineParameter("Correlation", {}, false, "1.0"));
}

bool RangeAccrualLegEngineBuilder::withSmile(const std::string& index) {
    return parseBool(engineParameter("WithSmile", {}, false, "false"));
}

bool RangeAccrualLegEngineBuilder::byCallSpread(const std::string& index) {
    return parseBool(engineParameter("ByCallSpread", {}, false, "true"));
}

} // namespace data
} // namespace ore
