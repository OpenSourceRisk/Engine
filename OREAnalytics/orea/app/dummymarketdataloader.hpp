/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file orea/app/dummymarketdataloader.hpp
    \brief Dummy Market Data Loader
*/

#pragma once

#include <orea/app/marketdataloader.hpp>
#include <ql/tuple.hpp>

namespace ore {
namespace analytics {

class DummyMarketDataLoader : public MarketDataLoader {
public:
    DummyMarketDataLoader(const QuantLib::ext::shared_ptr<InputParameters>& inputs) : MarketDataLoader(inputs) {}
    
    std::vector<std::pair<std::string, std::string>> marketDataQuotes() {
        std::vector<std::pair<std::string, std::string>> qts;
        for (const auto& [dt, names] : quotes()) {
            for (const auto& n : names)
                qts.push_back(std::make_pair(ore::data::to_string(dt), n));
        }
        return qts;
    };

    std::vector<std::pair<std::pair<std::string, std::string>, bool>> marketFixings() {
        std::vector<std::pair<std::pair<std::string, std::string>, bool>> fxgs;
        for (const auto& [name, fixingDates] : fixings()) {
            for (const auto& [date, mandatory] : fixingDates)
                fxgs.push_back(std::make_pair(std::make_pair(ore::data::to_string(date), name), mandatory));
        }
        return fxgs;
    }
    
};

} // namespace analytics
} // namespace ore
