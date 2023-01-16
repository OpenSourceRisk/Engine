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
// FIXME post credit migration
// #include <orepbase/ored/portfolio/referencedata.hpp>
// #include <orepcredit/ored/portfolio/indexcreditdefaultswap.hpp>
// #include <orepcredit/ored/portfolio/indexcreditdefaultswapoption.hpp>

using namespace ore::data;
using QuantExt::OptionPriceSurface;

namespace {

// Additional quotes are needed for backtesting, just for Credit Indexes for now
void additional_quotes(set<string>& quotes, const boost::shared_ptr<Portfolio>& portfolio,
                       const boost::shared_ptr<CurveConfigurations>& configs) {
    for (auto t : portfolio->trades()) {
        string cdsIndex = "";
        // FIXME post credit migration
        /*
        if (t->tradeType() == "IndexCreditDefaultSwap") {
            boost::shared_ptr<oreplus::data::IndexCreditDefaultSwap> icds =
                boost::dynamic_pointer_cast<oreplus::data::IndexCreditDefaultSwap>(t);
            if (icds)
                cdsIndex = icds->swap().creditCurveId();
        } else if (t->tradeType() == "IndexCreditDefaultSwapOption") {
            boost::shared_ptr<IndexCreditDefaultSwapOption> icdso =
                boost::dynamic_pointer_cast<IndexCreditDefaultSwapOption>(t);
            if (icdso)
                cdsIndex = icdso->swap().creditCurveId();
        }
        if (cdsIndex != "") {
            vector<string> qts = configs->defaultCurveConfig(cdsIndex)->quotes();
            for (auto q : qts)
                quotes.insert(q);
        }
        */
    }
}

// Additional quotes for fx fixings, add fixing quotes with USD and EUR to the list of fixings
// requested in order to triangulate missing fixings
void additional_fx_fixings(const string& fixingId, const set<Date>& fixingDates,
                           map<string, set<Date>>& relevantFixings) {
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
void additional_commodity_fixings(const string& fixingId, const set<Date>& fixingDates,
    map<string, set<Date>>& fixings, map<pair<string, Date>, set<Date>>& commodityMap) {
    
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

} // namespace

namespace ore {
namespace analytics {

void MarketDataLoader::populateLoader(
    const std::vector<boost::shared_ptr<ore::data::TodaysMarketParameters>>& todaysMarketParameters,
    bool doBacktest, bool doNPVLagged, const QuantLib::Date& npvLaggedDate, bool includeMporExpired) {

    // create a loader if doesn't already exist
    if (!loader_)
        loader_ = boost::make_shared<InMemoryLoader>();
    else
        loader_->reset(); // can only populate once to avoid duplicates
    
    // check input data
    QL_REQUIRE(!inputs_->curveConfigs().empty(), "Need at least one curve configuration to populate loader.");
    QL_REQUIRE(todaysMarketParameters.size() > 0, "No todaysMarketParams provided to populate market data loader.");
    
    // for equitites check if we have corporate action data
    map<string, string> equities;
    for (const auto& tmp : todaysMarketParameters) {
        if (tmp->hasMarketObject(MarketObject::EquityCurve)) {
            auto eqMap = tmp->mapping(MarketObject::EquityCurve, Market::defaultConfiguration);
            for (auto eq : eqMap) {
                // FIXME post credit migration
                // if (inputs_->refDataManager() && inputs_->refDataManager()->hasData("Equity", eq.first)) {
                //     auto refData = boost::dynamic_pointer_cast<EquityReferenceDatum>(
                //         inputs_->refDataManager()->getData("Equity", eq.first));
                //     auto erData = refData->equityData();
                //     equities[eq.first] = erData.equityId;
                // } else
                equities[eq.first] = eq.first;
            }
        }
    }
    if (equities.size() > 0)
        loadCorporateActionData(loader_, equities);
    
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
                if (isFxIndex(it.first)) {
                    // for FX fixings we want to add additional fixings to allow triangulation in case of missing
                    // fixings if we need EUR/GBP fixing but it is not available, we can imply from EUR/USD and GBP/USD
                    additional_fx_fixings(it.first, it.second, relevantFixings);
                }
                if (useMarketDataFixings_ && isEquityIndex(it.first)) {
                    auto ei = parseEquityIndex(it.first);
                    // look up the curveconfig to get the spot MD Id
                    if (inputs_->curveConfigs().at(0)->hasEquityCurveConfig(ei->name())) {
                        auto ecc = inputs_->curveConfigs().at(0)->equityCurveConfig(ei->name());
                        string mdQuote = ecc->equitySpotQuoteID();
                        mdFixings.add(mdQuote, it.second, it.first);
                    } else {
                        WLOG("Cannot find curve configurations for equity " << ei->name()
                                                                            << ", unable to get required fixings.")
                    }
                    continue;
                }
                if (useMarketDataFixings_ && isGenericIndex(it.first)) {
                    // for generic indexes we want send aything with name GENERIC-MD/... to market data
                    string::size_type pos = it.first.find_first_of('-');
                    string name;
                    if (pos != string::npos) {
                        name = it.first.substr(pos + 1);
                    }

                    string::size_type pos1 = name.find_first_of('/');
                    string mdQuote;
                    if (pos1 != string::npos) {
                        if (name.substr(0, pos1) == "MD")
                            mdQuote = name.substr(pos1 + 1);
                    }
                    if (!mdQuote.empty()) {
                        mdFixings.add(mdQuote, it.second, it.first);
                        continue;
                    }
                }
                if (isCommodityIndex(it.first)) {
                    additional_commodity_fixings(it.first, it.second, relevantFixings, lastAvailableFixingLookupMap);
                }
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
    
    if (doBacktest) { // set up only for backtest
        // if it's a SIMM backtest, we need additional quotes
        for (const auto& curveConfig : inputs_->curveConfigs()) {
            additional_quotes(quotes_, inputs_->portfolio(), curveConfig);
        }
    }

    // Get the relevant market data loader for the pricing call
    Date relabelMd = doNPVLagged ? inputs_->asof() : Date();
    retrieveMarketData(loader_, quotes_, relabelMd);
    if (doNPVLagged && includeMporExpired)
            addExpiredContracts(*loader_, quotes_, npvLaggedDate);
    LOG("Got market data");
}

void MarketDataLoader::implyMarketDataFixings(const boost::shared_ptr<ore::data::InMemoryLoader>& loader,
                                              MarketDataFixings missingFixings) {

    // get the fixings by date
    map<Date, set<string>> fixings;

    for (auto f : missingFixings.fixings()) {
        for (auto d : f.second.dates) {
            // only handle Equity option fixings
            if (boost::starts_with(f.first, "EQUITY_OPTION/"))
                fixings[d].insert(f.first);
        }
    }

    // create a loader just for the market data pulled down here, we don't need to keep this
    boost::shared_ptr<InMemoryLoader> tempLoader = boost::make_shared<InMemoryLoader>();

    for (const auto& fd : fixings) {
        DLOG("MarketDataLoader: Implying Market data fixings for date " << fd.first);

        // the quote stems with wildcards, e.g: EQUITY_OPTION/PRICE/RIC:.SPX/USD/.*/.*/C
        set<string> quotes;
        // store a map from a quote stem, option type pair to the MD Fixing, since we could
        // need multiple fixings for the same quote stem, this allows us to build one surface
        // to look up all these
        map<pair<string, string>, set<string>> quoteToFixing;

        for (const auto& f : fd.second) {
            // only valid for equity options
            std::vector<std::string> tokens;
            boost::split(tokens, f, boost::is_any_of("/"));

            std::ostringstream oss;
            std::copy(tokens.begin(), tokens.begin() + 4, std::ostream_iterator<string>(oss, "/"));
            oss << "*";
            quotes.insert(oss.str());
            quoteToFixing[make_pair(oss.str(), tokens[6])].insert(f);
        }

        // load the market data
        retrieveMarketData(tempLoader, quotes, fd.first);

        // loop over the quote stems and build a price surface for each quote
        for (const auto& qs : quoteToFixing) {
            string q = qs.first.first;
            vector<Real> strikes, data;
            vector<Date> expiries;
            Date asof = fd.first;

            std::vector<std::string> tokens;
            boost::split(tokens, q, boost::is_any_of("/"));

            std::ostringstream ss;
            ss << MarketDatum::InstrumentType::EQUITY_OPTION << "/" << MarketDatum::QuoteType::PRICE << "/*";
            Wildcard w(ss.str());
            for (const auto& md : tempLoader->get(w, asof)) {
                QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");

                boost::shared_ptr<EquityOptionQuote> eoq = boost::dynamic_pointer_cast<EquityOptionQuote>(md);
                QL_REQUIRE(eoq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to EquityOptionQuote");

                auto absoluteStrike = boost::dynamic_pointer_cast<AbsoluteStrike>(eoq->strike());
                if (absoluteStrike && (eoq->eqName() == tokens[2] && eoq->ccy() == tokens[3])) {
                    // filter for just calls or puts, can't mix surfaces
                    if (eoq->isCall() == (qs.first.second == "C")) {
                        Calendar cal = parseCalendar(tokens[3]);
                        Date tmpDate = getDateFromDateOrPeriod(eoq->expiry(), asof, cal);
                        if (tmpDate <= asof) {
                            LOG("MarketDataLoader::implyMarketDataFixings: Expired Equity volatility quote '"
                                << eoq->name() << "' ignored, expired on (" << io::iso_date(tmpDate) << ")");
                            continue;
                        }
                        // get values and strikes, convert from minor to major currency if needed
                        Real quoteValue = eoq->quote()->value();
                        quoteValue = convertMinorToMajorCurrency(eoq->ccy(), quoteValue);
                        Real strikeValue = convertMinorToMajorCurrency(eoq->ccy(), absoluteStrike->strike());

                        strikes.push_back(strikeValue);
                        data.push_back(quoteValue);
                        expiries.push_back(tmpDate);
                    }
                }
            }

            if (!expiries.empty() && !strikes.empty() && !data.empty()) {
                try {
                    boost::shared_ptr<OptionPriceSurface> surface =
                        boost::make_shared<OptionPriceSurface>(asof, expiries, strikes, data, Actual365Fixed());

                    // now loop over each fixing for this quote and look up its value
                    for (const auto& f : qs.second) {
                        std::vector<std::string> fixTokens;
                        boost::split(fixTokens, f, boost::is_any_of("/"));
                        Date fixingExpiry = parseDate(fixTokens[4]);
                        Real fixingStrike = parseReal(fixTokens[5]);
                        Real fixingValue = surface->getValue(fixingExpiry, fixingStrike);

                        // update the loader with fixing, for all fixing names mapped to this market data name
                        if (fixingValue != Null<Real>() && missingFixings.has(f)) {
                            for (const auto& n : missingFixings.get(f).fixingNames)
                                loader->addFixing(asof, n, fixingValue);
                        }
                    }
                } catch (const std::exception& e) {
                    WLOG("MarketDataLoader::implyMarketDataFixings: Failed to imply fixing for " << q << ". Got "
                                                                                                 << e.what());
                }
            } else {
                WLOG("MarketDataLoader::implyMarketDataFixings: Failed to build option surface for "
                     << q << ", found no quotes");
            }
        }
    }
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
