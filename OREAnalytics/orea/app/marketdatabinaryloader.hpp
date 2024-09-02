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

/*! \file orea/app/marketdatabinaryloader.hpp
    \brief Market Data Loader
*/

#pragma once

#include <orea/app/marketdataloader.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

namespace ore {
namespace analytics {

class MarketDataBinaryLoader : public MarketDataLoader {
public: 
    MarketDataBinaryLoader(const QuantLib::ext::shared_ptr<InputParameters>& inputs,
                        const std::string& file)
        : MarketDataLoader(inputs, nullptr), file_(file) {}

    virtual void populateLoader(
        const std::vector<QuantLib::ext::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
        const std::set<QuantLib::Date>& loaderDates) override{
        
        ore::data::InMemoryLoader loader;
        
        LOG("Deserialize market data loader from '" << file_ << "'");
        std::ifstream is(file_, std::ios::binary);
        boost::archive::binary_iarchive ia(is, boost::archive::no_header);
        ia >> loader;
        is.close();

        loader_ = QuantLib::ext::make_shared<ore::data::InMemoryLoader>(loader);
        LOG("Market data loading complete from file '" << file_ << "'");
    };

private:
    std::string file_;
};
    
} // namespace analytics
} // namespace ore

