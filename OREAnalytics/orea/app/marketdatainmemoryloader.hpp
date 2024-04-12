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

/*! \file orea/app/marketdatainmemoryloader.hpp
    \brief Market Data Loader
*/

#pragma once

#include <orea/app/marketdataloader.hpp>
#include <ored/marketdata/inmemoryloader.hpp>

namespace ore {
namespace analytics {
    
class MarketDataInMemoryLoaderImpl : public MarketDataLoaderImpl {
public:
    MarketDataInMemoryLoaderImpl() {}

    MarketDataInMemoryLoaderImpl(const QuantLib::ext::shared_ptr<InputParameters>& inputs, 
                                 const std::vector<std::string>& marketData,
                                 const std::vector<std::string>& fixingData)
        : inputs_(inputs), marketData_(marketData), fixingData_(fixingData) {}
    
    void loadCorporateActionData(QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
                                 const std::map<std::string, std::string>& equities) override;
    
    void retrieveMarketData(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        const ore::analytics::QuoteMap& quotes,
        const QuantLib::Date& marketDate) override;        
    
    void retrieveFixings(const QuantLib::ext::shared_ptr<ore::data::InMemoryLoader>& loader,
        ore::analytics::FixingMap fixings = {},
        std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>> lastAvailableFixingLookupMap = {}) override;

private:
    QuantLib::ext::shared_ptr<InputParameters> inputs_;
    std::vector<std::string> marketData_;
    std::vector<std::string> fixingData_;
};

class MarketDataInMemoryLoader : public MarketDataLoader {
public: 
    MarketDataInMemoryLoader(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                             const std::vector<std::string>& marketData,
                             const std::vector<std::string>& fixingData)
        : MarketDataLoader(inputs, QuantLib::ext::make_shared<MarketDataInMemoryLoaderImpl>(inputs, marketData, fixingData)) {}
};
    
} // namespace analytics
} // namespace ore
