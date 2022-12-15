/*
 Copyright (C) 2022 Quaternion Risk Management Ltd
 All rights reserved.
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

    //! Imply missing market data fixings
    void implyMarketDataFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                ore::analytics::MarketDataFixings missingFixings);

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
        
    /*! Look up and add contracts that expired between \c asofDate_ and \p npvLaggedDate when populating the loader
        for a lagged NPV run. We currently only need this for commodity future contracts.
    */
    virtual void addExpiredContracts(ore::data::InMemoryLoader& loader, const std::set<std::string>& quotes, 
        const QuantLib::Date& npvLaggedDate) = 0;
};


} // namespace analytics
} // namespace oreplus
