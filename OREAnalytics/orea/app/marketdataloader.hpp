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
#include <ored/utilities/to_string.hpp>

namespace ore {
namespace analytics {

typedef std::map<QuantLib::Date, std::set<std::string>> QuoteMap;
typedef std::map<std::string, std::set<QuantLib::Date>> FixingMap;

//! Utility class for Structured Fixing warnings
class StructuredFixingWarningMessage : public StructuredMessage {
public:
    StructuredFixingWarningMessage(const std::string& fixingId, const QuantLib::Date& fixingDate,
        const std::string& exceptionType, const std::string& exceptionWhat = "")
        : StructuredMessage(Category::Warning, Group::Fixing, exceptionWhat, 
            std::map<std::string, std::string>({{"exceptionType", exceptionType}, {"fixingId", fixingId}, 
                {"fixingDate", ore::data::to_string(fixingDate)}})) {}
};

class MarketDataLoaderImpl {
public:
    MarketDataLoaderImpl() {}
    virtual ~MarketDataLoaderImpl() {}

    //! load corporate action data
    virtual void loadCorporateActionData(boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const std::map<std::string, std::string>& equities) = 0;

    //! retrieve market data
    virtual void retrieveMarketData(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, 
        const QuoteMap& quotes, const QuantLib::Date& relabelDate = QuantExt::Null<QuantLib::Date>()) = 0;

    //! retrieve fixings
    virtual void retrieveFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader, FixingMap fixings = {}, 
        std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>> lastAvailableFixingLookupMap = {}) = 0;

    //! if running a lagged market we may need to load addition contracts
    virtual void retrieveExpiredContracts(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                          const QuoteMap& quotes, const Date& npvLaggedDate) = 0;
};

class MarketDataLoader {
public:
    MarketDataLoader(const boost::shared_ptr<InputParameters>& inputs, boost::shared_ptr<MarketDataLoaderImpl> impl)
        : inputs_(inputs), impl_(impl) {
        loader_ = boost::make_shared<ore::data::InMemoryLoader>();
    }
    virtual ~MarketDataLoader() {}

    /*! Populate a market data \p loader. Gathers all the quotes needed based on the configs provided and calls the
        marketdata and fixing services
    */
    void populateLoader(const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
        bool doNPVLagged = false, const QuantLib::Date& npvLaggedDate = QuantLib::Date(), bool includeMporExpired = true);

    virtual void populateFixings(const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters);

    virtual void addRelevantFixings(const std::pair<std::string, std::set<QuantLib::Date>>& fixing,
        std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>>& lastAvailableFixingLookupMap);

    //! clear the loader
    void resetLoader() { loader_ = boost::make_shared<ore::data::InMemoryLoader>(); };

    //! getters
    const boost::shared_ptr<ore::data::InMemoryLoader>& loader() const { return loader_; };
    QuoteMap quotes() { return quotes_; }

protected:
    boost::shared_ptr<InputParameters> inputs_;
    boost::shared_ptr<ore::data::InMemoryLoader> loader_;
    QuoteMap quotes_;
    FixingMap portfolioFixings_, fixings_;

    const boost::shared_ptr<MarketDataLoaderImpl>& impl() const;

private:
    boost::shared_ptr<MarketDataLoaderImpl> impl_;
};

} // namespace analytics
} // namespace ore
