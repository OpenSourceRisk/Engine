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

#include <orea/app/marketdataloader.hpp>
#include <qle/termstructures/optionpricesurface.hpp>
// #include <orepbase/ored/portfolio/referencedata.hpp>
// #include <orepcredit/ored/portfolio/indexcreditdefaultswap.hpp>
// #include <orepcredit/ored/portfolio/indexcreditdefaultswapoption.hpp>

using namespace ore::data;
using QuantExt::OptionPriceSurface;

namespace ore {
namespace analytics {

void MarketDataLoader::populateLoader(
    const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
    bool doSimmBacktest, bool doNPVLagged, const QuantLib::Date& npvLaggedDate, bool includeMporExpired) {

    // create a loader if doesn't already exist
    if (!loader_)
        loader_ = boost::make_shared<InMemoryLoader>();
    else
        loader_->reset(); // can only populate once to avoid duplicates

    // check input data
    QL_REQUIRE(!inputs_->curveConfigs().empty(), "Need at least one curve configuration to populate loader.");
    QL_REQUIRE(todaysMarketParameters.size() > 0, "No todaysMarketParams provided to populate market data loader.");
        
    MarketDataFixings mdFixings;
    if (inputs_->allFixings()) {
        retrieveFixings(loader_);
    } else {
        LOG("Asking portfolio for its required fixings");
        map<string, set<Date>> portfolioFixings, relevantFixings;
        map<pair<string, Date>, set<Date>> lastAvailableFixingLookupMap;

        if (inputs_->portfolio()) {
            portfolioFixings = inputs_->portfolio()->fixings();
            LOG("The portfolio depends on fixings from " << portfolioFixings.size() << " indices");
            for (auto it : portfolioFixings) {
                // ...
                relevantFixings[it.first].insert(it.second.begin(), it.second.end());
            }
        }

        LOG("Add fixings possibly required for bootstrapping TodaysMarket");
        for (const auto& tmp : todaysMarketParameters)
            addMarketFixingDates(relevantFixings, *tmp);

        if (inputs_->eomInflationFixings()) {
            LOG("Adjust inflation fixing dates to the end of the month before the request");
            amendInflationFixingDates(relevantFixings);
        }

        if (relevantFixings.size() > 0)
            retrieveFixings(loader_, relevantFixings, lastAvailableFixingLookupMap);

        if (mdFixings.size() > 0) {
            MarketDataFixings missingFixings;
            missingFixings = retrieveMarketDataFixings(loader_, mdFixings);

            // if we have missing fixings, try to imply them
            if (missingFixings.size() > 0)
                implyMarketDataFixings(loader_, missingFixings);
        }
    }

    LOG("Adding the loaded fixings to the IndexManager");
    applyFixings(loader_->loadFixings());

    // Get set of quotes we need
    LOG("Generating market datum set");
    for (const auto& tmp : todaysMarketParameters) {
        // Find all configurations in this todaysMarket
        set<string> configurations;
        for (auto c : tmp->configurations())
            configurations.insert(c.first);

        for (const auto& curveConfig : inputs_->curveConfigs()) {
            auto qs = curveConfig->quotes(tmp, configurations);
            quotes_.insert(qs.begin(), qs.end());
        }
    }
    LOG("CurveConfigs require " << quotes_.size() << " quotes");
    
    retrieveMarketData(loader_, quotes_);

    LOG("Got market data");
}
    
void MarketDataFixings::add(const string& mdName, const set<Date>& dates, const string& fixingName) {
    if (!has(mdName))
        mdFixings_[mdName] = FixingInfo();

    mdFixings_[mdName].dates.insert(dates.begin(), dates.end());
    mdFixings_[mdName].fixingNames.insert(fixingName);
}

void MarketDataFixings::add(const std::string& mdName, const FixingInfo& fixingInfo) {
    if (has(mdName)) {
        mdFixings_[mdName].dates.insert(fixingInfo.dates.begin(), fixingInfo.dates.end());
        mdFixings_[mdName].fixingNames.insert(fixingInfo.fixingNames.begin(), fixingInfo.fixingNames.end());
    } else
        mdFixings_[mdName] = fixingInfo;
}

bool MarketDataFixings::has(const string& mdName) const { return mdFixings_.find(mdName) != mdFixings_.end(); }

MarketDataFixings::FixingInfo MarketDataFixings::get(const std::string& mdName) const {
    auto it = mdFixings_.find(mdName);
    QL_REQUIRE(it != mdFixings_.end(), "MarketDataFixings: Could not find market data fixing " << mdName);
    return it->second;
}

Size MarketDataFixings::size() const { return mdFixings_.size(); }

bool MarketDataFixings::empty() const { return mdFixings_.empty(); }

map<string, MarketDataFixings::FixingInfo> MarketDataFixings::fixings() const { return mdFixings_; }

}
} // namespace oreplus
