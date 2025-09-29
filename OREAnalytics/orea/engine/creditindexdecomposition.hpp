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
#pragma once
#include <ored/portfolio/trade.hpp>
#include <ored/marketdata/market.hpp>


namespace ore {
namespace analytics {

void decomposeCreditIndex(const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                          const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                          std::map<std::string, QuantLib::Real>& sensitivityDecompositionWeights, bool& decompose);

} // namespace analytics
} // namespace ore