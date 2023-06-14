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
#include <ored/portfolio/indexcreditdefaultswap.hpp>
#include <ored/portfolio/indexcreditdefaultswapoption.hpp>
#include <ored/utilities/to_string.hpp>
#include <ored/utilities/currencyhedgedequityindexdecomposition.hpp>

using namespace ore::data;
using QuantExt::OptionPriceSurface;

namespace ore {
namespace analytics {

// Additional quotes for fx fixings, add fixing quotes with USD and EUR to the list of fixings
// requested in order to triangulate missing fixings
void additional_fx_fixings(const string& fixingId, const set<Date>& fixingDates, FixingMap& relevantFixings) {
    std::vector<std::string> tokens;
    boost::split(tokens, fixingId, boost::is_any_of("-"));
    QL_REQUIRE(tokens.size() == 4, "MarketDataLoader::additional_fx_fixings: Invalid fixing id, "
                                       << "must be of form FX-TYPE-CCY1-CCY, e.g FX-ECB-EUR-GBP");

    // add fixings on inverted ccy pair
    relevantFixings[tokens[0] + "-" + tokens[1] + "-" + tokens[3] + "-" + tokens[2]].insert(fixingDates.begin(),
                                                                                            fixingDates.end());

    vector<string> baseCcys = {"USD", "EUR"};

    for (auto ccy : baseCcys) {

        string fixingType = tokens[0] + "-" + tokens[1] + "-";
        if (tokens[2] != ccy) {
            string fix = fixingType + ccy + "-" + tokens[2];
            relevantFixings[fix].insert(fixingDates.begin(), fixingDates.end());

            if (tokens[3] != ccy) {
                fix = fixingType + tokens[2] + "-" + ccy;
                relevantFixings[fix].insert(fixingDates.begin(), fixingDates.end());
            }
        }

        if (tokens[3] != ccy) {
            string fix = fixingType + ccy + "-" + tokens[3];
            relevantFixings[fix].insert(fixingDates.begin(), fixingDates.end());

            if (tokens[2] != ccy) {
                fix = fixingType + tokens[3] + "-" + ccy;
                relevantFixings[fix].insert(fixingDates.begin(), fixingDates.end());
            }
        }
    }
}

// Additional quotes for commodity fixings
void additional_commodity_fixings(const string& fixingId, const set<Date>& fixingDates, FixingMap& fixings,
    map<pair<string, Date>, set<Date>>& commodityMap) {
    
    boost::shared_ptr<Conventions> conventions = InstrumentConventions::instance().conventions();

    auto ind = parseCommodityIndex(fixingId);
    string commName = ind->underlyingName();

    boost::shared_ptr<CommodityFutureConvention> cfc;
    if (conventions->has(commName)) {
        cfc = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions->get(commName));
    }

    if (cfc) {
        // Add historical fixings for daily and monthly expiring contracts
        // TODO: potentially need to add for OffPeakPowerIndex commodities too.
        if (cfc->contractFrequency() == Daily) {
            for (const auto& fd : fixingDates) {
                // Add 1 week lookback for each date for daily expiry
                set<Date> dates;
                Date wLookback = fd - Period(1, Weeks);
                do {
                    dates.insert(wLookback++);
                } while (wLookback <= fd);

                TLOG("Adding (date, id) = (" << io::iso_date(fd) << "," << fixingId << ")");
                // Add to the fixings so a fixing is requested for all dates, and also to the commodityMap
                // so we can map a fixing to the correct date required
                fixings[fixingId].insert(dates.begin(), dates.end());
                commodityMap[pair(fixingId, fd)].insert(dates.begin(), dates.end());
            }
        } else {
            // for monthly expiries add fixings for the last 45 days
            for (const auto& fd : fixingDates) {
                set<Date> dates;
                Date wLookback = fd - Period(45, Days);
                do {
                    dates.insert(wLookback++);
                } while (wLookback <= fd);

                TLOG("Adding (date, id) = (" << io::iso_date(fd) << "," << fixingId << ")");
                // Add to the fixings so a fixing is requested for all dates, and also to the commodityMap
                // so we can map a fixing to the correct date required
                fixings[fixingId].insert(dates.begin(), dates.end());
                commodityMap[pair(fixingId, fd)].insert(dates.begin(), dates.end());
            }
        }
    }
}

// Additional fixings for equity index decomposition
void additional_equity_fixings(map<string, set<Date>>& fixings, const TodaysMarketParameters& mktParams,
                               const boost::shared_ptr<ReferenceDataManager> refData,
                               const boost::shared_ptr<CurveConfigurations>& curveConfigs) {
    std::string configuration = Market::defaultConfiguration;
    Date asof = Settings::instance().evaluationDate();
    boost::shared_ptr<CurrencyHedgedEquityIndexReferenceDatum> currencyHedgedIndexData;
    if (mktParams.hasMarketObject(MarketObject::EquityCurve)) {
        for (const auto& [equityName, _] : mktParams.mapping(MarketObject::EquityCurve, configuration)) {
            try {    
                auto indexDecomposition = loadCurrencyHedgedIndexDecomposition(equityName, refData, curveConfigs);
                if (indexDecomposition) {
                    indexDecomposition->addAdditionalFixingsForEquityIndexDecomposition(asof, fixings);
                }
            } catch (const std::exception& e) {
                ALOG("adding addtional equity fixing failed, " << e.what());
            }
        }
    }
}

const boost::shared_ptr<MarketDataLoaderImpl>& MarketDataLoader::impl() const {
    QL_REQUIRE(impl_, "No MarketDataLoader implementation of type MarketDataLoaderImpl set");
    return impl_;
}

void MarketDataLoader::addRelevantFixings(
    const std::pair<std::string, std::set<QuantLib::Date>>& fixing,
    std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>>& lastAvailableFixingLookupMap) {
    if (isFxIndex(fixing.first)) {
        // for FX fixings we want to add additional fixings to allow triangulation in case of missing
        // fixings if we need EUR/GBP fixing but it is not available, we can imply from EUR/USD and GBP/USD
        additional_fx_fixings(fixing.first, fixing.second, fixings_);
    }
    if (isCommodityIndex(fixing.first)) {
        additional_commodity_fixings(fixing.first, fixing.second, fixings_, lastAvailableFixingLookupMap);
    }
    fixings_[fixing.first].insert(fixing.second.begin(), fixing.second.end());
    // also add to portfolioFixings_, used for alerting missing required fixings
    portfolioFixings_[fixing.first].insert(fixing.second.begin(), fixing.second.end());
}

void MarketDataLoader::populateFixings(
    const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters) {
    if (inputs_->allFixings()) {
        impl()->retrieveFixings(loader_);
    } else {
        LOG("Asking portfolio for its required fixings");
        FixingMap portfolioFixings;
        std::map<std::pair<std::string, QuantLib::Date>, std::set<QuantLib::Date>> lastAvailableFixingLookupMap;
        
        // portfolio fixings will warn if missinbg
        if (inputs_->portfolio()) {
            portfolioFixings = inputs_->portfolio()->fixings();
            LOG("The portfolio depends on fixings from " << portfolioFixings.size() << " indices");
            for (const auto& it : portfolioFixings)
                addRelevantFixings(it, lastAvailableFixingLookupMap);
        }

        LOG("Add fixings possibly required for bootstrapping TodaysMarket");
        for (const auto& tmp : todaysMarketParameters) {
            addMarketFixingDates(fixings_, *tmp);
            LOG("Add fixing possibly required for equity index delta risk decomposition")
            additional_equity_fixings(fixings_, *tmp, inputs_->refDataManager(),
                                  inputs_->curveConfigs().front());
        }

        if (inputs_->eomInflationFixings()) {
            LOG("Adjust inflation fixing dates to the end of the month before the request");
            amendInflationFixingDates(fixings_);
            amendInflationFixingDates(portfolioFixings_);
        }

        if (fixings_.size() > 0)
            impl()->retrieveFixings(loader_, fixings_, lastAvailableFixingLookupMap);
        
        // apply all fixings now
        applyFixings(loader_->loadFixings());

        // check and warn any missing fixings - only warn for portfolio fixings
        for (const auto& f : portfolioFixings_) {
            for (const auto& d : f.second) {
                if (!loader_->hasFixing(f.first, d)) {
                    string fixingErr = "";
                    if (isFxIndex(f.first)) {
                        auto fxInd = parseFxIndex(f.first);
                        try { 
                            if(fxInd->fixingCalendar().isBusinessDay(d))
                                fxInd->fixing(d);
                            break;
                        } catch (const std::exception& e) {
                            fixingErr = ", error: " + ore::data::to_string(e.what());
                        }
                    }
                    WLOG(StructuredFixingWarningMessage(f.first, d, "Missing fixing",
                        "Could not find required fixing id " + f.first +
                        " for date " + ore::data::to_string(d) + fixingErr));
                }
            }
        }
    }
}

void MarketDataLoader::populateLoader(
    const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
    const std::set<QuantLib::Date>& loaderDates) {

    // create a loader if doesn't already exist
    if (!loader_)
        loader_ = boost::make_shared<InMemoryLoader>();
    else
        loader_->reset(); // can only populate once to avoid duplicates

    // check input data
    QL_REQUIRE(!inputs_->curveConfigs().empty(), "Need at least one curve configuration to populate loader.");
    QL_REQUIRE(todaysMarketParameters.size() > 0, "No todaysMarketParams provided to populate market data loader.");

    // for equitites check if we have corporate action data
    std::map<std::string, std::string> equities;
    for (const auto& tmp : todaysMarketParameters) {
        if (tmp->hasMarketObject(MarketObject::EquityCurve)) {
            auto eqMap = tmp->mapping(MarketObject::EquityCurve, Market::defaultConfiguration);
            for (auto eq : eqMap) {
                if (inputs_->refDataManager() && inputs_->refDataManager()->hasData("Equity", eq.first)) {
                    auto refData = boost::dynamic_pointer_cast<EquityReferenceDatum>(
                        inputs_->refDataManager()->getData("Equity", eq.first));
                    auto erData = refData->equityData();
                    equities[eq.first] = erData.equityId;
                } else
                    equities[eq.first] = eq.first;
            }
        }
    }
    if (equities.size() > 0)
        impl()->loadCorporateActionData(loader_, equities);

    // apply dividends now
    applyDividends(loader_->loadDividends());

    populateFixings(todaysMarketParameters);

    LOG("Adding the loaded fixings to the IndexManager");
    applyFixings(loader_->loadFixings());

    // Get set of quotes we need
    LOG("Generating market datum set");
    set<string> quotes;
    for (const auto& tmp : todaysMarketParameters) {
        // Find all configurations in this todaysMarket
        std::set<std::string> configurations;
        for (auto c : tmp->configurations())
            configurations.insert(c.first);

        for (const auto& curveConfig : inputs_->curveConfigs()) {
            auto qs = curveConfig->quotes(tmp, configurations);
            quotes.insert(qs.begin(), qs.end());
        }
    }

    for (const auto& d : loaderDates) {
        QuoteMap quoteMap;
        quoteMap[d] = quotes;

        LOG("CurveConfigs require " << quotes.size() << " quotes");

        // Get the relevant market data loader for the pricing call
        impl()->retrieveMarketData(loader_, quoteMap, d);

        quotes_[d] = quotes;
    }
    LOG("Got market data");
}

}
} // namespace oreplus
