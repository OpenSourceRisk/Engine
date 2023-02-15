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
    
class MarketDataCsvLoaderImpl : public MarketDataLoaderImpl {
public:
    MarketDataCsvLoaderImpl() {}

    MarketDataCsvLoaderImpl(const boost::shared_ptr<InputParameters>& inputs, 
        const boost::shared_ptr<ore::data::CSVLoader>& csvLoader)
        : inputs_(inputs), csvLoader_(csvLoader) {}
    
    void loadCorporateActionData(boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                 const std::map<std::string, std::string>& equities) override;
    
    void retrieveMarketData(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
        const ore::analytics::QuoteMap& quotes,
        const QuantLib::Date& relabelDate = QuantExt::Null<QuantLib::Date>()) override;        
    
    void retrieveFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
        ore::analytics::FixingMap fixings = {},
        std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>> lastAvailableFixingLookupMap = {}) override;

    void retrieveExpiredContracts(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, const QuoteMap& quotes,
                             const Date& npvLaggedDate) override {};

private:
    boost::shared_ptr<InputParameters> inputs_;
    boost::shared_ptr<ore::data::CSVLoader> csvLoader_;
};

class MarketDataCsvLoader : public MarketDataLoader {
public: 
    MarketDataCsvLoader(const boost::shared_ptr<InputParameters>& inputs,
                        const boost::shared_ptr<ore::data::CSVLoader>& csvLoader)
        : MarketDataLoader(inputs, boost::make_shared<MarketDataCsvLoaderImpl>(inputs, csvLoader)) {}
};
    
} // namespace analytics
} // namespace ore
