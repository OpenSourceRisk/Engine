/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source librar
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

#include <ored/portfolio/builders/asianoption.hpp>
#include <ored/portfolio/asianoption.hpp>

namespace ore {
namespace data {

QuantLib::ext::shared_ptr<ore::data::Trade>
AsianOptionScriptedEngineBuilder::build(const Trade* trade, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory) {

    auto asianOption = dynamic_cast<const ore::data::AsianOption*>(trade);

    QL_REQUIRE(
        asianOption != nullptr,
        "AsianOptionScriptedEngineBuilder: internal error, could not cast to ore::data::AsianOption. Contact dev.");

    auto basketOption = QuantLib::ext::make_shared<BasketOption>(
        asianOption->payCurrency(), std::to_string(asianOption->quantity()), asianOption->strike(),
        std::vector<QuantLib::ext::shared_ptr<ore::data::Underlying>>(1, asianOption->underlying()), asianOption->option(),
        asianOption->settlementDate() == QuantLib::Null<QuantLib::Date>()
            ? ""
            : ore::data::to_string(asianOption->settlementDate()),
        asianOption->observationDates());

    basketOption->build(engineFactory);

    return basketOption;
}

} // namespace data
} // namespace ore
