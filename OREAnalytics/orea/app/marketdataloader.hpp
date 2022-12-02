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

/*! \file orea/app/marketdataloader.hpp
    \brief Market Data Loader
*/

#pragma once

#include <ored/marketdata/inmemoryloader.hpp>
#include <orea/app/inputparameters.hpp>

namespace ore {
namespace analytics {

//! A class to hold market data Fixings
class MarketDataFixings {
public:
    struct FixingInfo {
        std::set<QuantLib::Date> dates;
        std::set<std::string> fixingNames;
    };

    MarketDataFixings(){};

    void add(const std::string& mdName, const std::set<QuantLib::Date>& dates, const std::string& fixingName);
    void add(const std::string& mdName, const FixingInfo& fixingInfo);
    std::map<std::string, FixingInfo> fixings() const;

    bool has(const std::string& mdName) const;
    FixingInfo get(const std::string& mdName) const;

    QuantLib::Size size() const;
    bool empty() const;

private:
    std::map<std::string, FixingInfo> mdFixings_;
};

class MarketDataLoader {
public:
    MarketDataLoader(const boost::shared_ptr<InputParameters>& inputs, const bool useMarketDataFixings = false)
        : inputs_(inputs), useMarketDataFixings_(useMarketDataFixings) {
        loader_ = boost::make_shared<ore::data::InMemoryLoader>();
    }
    virtual ~MarketDataLoader() {}

    /*! Populate a market data \p loader. Gathers all the quotes needed based on the configs provided and calls the
        marketdata and fixing services
    */
    void populateLoader(const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
                        bool doSimmBacktest = false, bool doNPVLagged = false,
                        const QuantLib::Date& npvLaggedDate = QuantLib::Date(), bool includeMporExpired = true);

    //! load corporate action data
    virtual void loadCorporateActionData(boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                         const std::map<std::string, std::string>& equities) = 0;

    //! retrieve market data
    virtual void retrieveMarketData(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                    const std::set<std::string>& quotes,
                                    const QuantLib::Date& relabelDate = QuantExt::Null<QuantLib::Date>()) = 0;
    //! retrieve fixings
    virtual void retrieveFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                 std::map<std::string, std::set<QuantLib::Date>> fixings = {},
                                 std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>> lastAvailableFixingLookupMap = {}) = 0;

    //! retrieve fixings that come from market data
    virtual MarketDataFixings retrieveMarketDataFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                           const MarketDataFixings& mdFixings) = 0;  

    //! imply missing market data fixings, override this to do some magic gap filling
    virtual void implyMarketDataFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                        ore::analytics::MarketDataFixings missingFixings) {};

    //! clear the loader
    void resetLoader() { loader_ = boost::make_shared<ore::data::InMemoryLoader>(); };

    //! getters
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader() const { return loader_; };
    std::set<std::string> quotes() { return quotes_; }

protected:
    boost::shared_ptr<InputParameters> inputs_;
    bool useMarketDataFixings_;
    boost::shared_ptr<ore::data::InMemoryLoader> loader_;
    std::set<std::string> quotes_;
};
    
} // namespace analytics
} // namespace ore
