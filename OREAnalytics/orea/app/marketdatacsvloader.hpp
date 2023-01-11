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

/*! \file orea/app/marketdatacsvloader.hpp
    \brief Market Data Loader
*/

#pragma once

#include <orea/app/marketdataloader.hpp>
#include <ored/marketdata/csvloader.hpp>

namespace ore {
namespace analytics {
using namespace ore::data;
    
class MarketDataCsvLoader : public ore::analytics::MarketDataLoader {
public:
    MarketDataCsvLoader(const boost::shared_ptr<ore::analytics::InputParameters>& inputParameters,
        const boost::shared_ptr<CSVLoader>& csvLoader)
        : MarketDataLoader(inputParameters), csvLoader_(csvLoader) {}
    
    void loadCorporateActionData(boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                 const std::map<std::string, std::string>& equities) override;
    
    void retrieveMarketData(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                            const std::set<std::string>& quotes,
                            const QuantLib::Date& relabelDate = QuantExt::Null<QuantLib::Date>()) override;
        
    
    void retrieveFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                         std::map<std::string, std::set<QuantLib::Date>> fixings = {},
                         map<pair<string, Date>, set<Date>> lastAvailableFixingLookupMap = {}) override; 

    MarketDataFixings retrieveMarketDataFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                                const MarketDataFixings& mdFixings) override;  

    void addExpiredContracts(ore::data::InMemoryLoader& loader, const std::set<std::string>& quotes, 
                             const QuantLib::Date& npvLaggedDate) override;

private:
    boost::shared_ptr<CSVLoader> csvLoader_;
};


    
} // namespace analytics
} // namespace ore
