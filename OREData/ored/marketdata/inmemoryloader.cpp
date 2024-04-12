/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <boost/algorithm/string.hpp>
#include <ored/marketdata/inmemoryloader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

namespace {
QuantLib::ext::shared_ptr<MarketDatum> makeDummyMarketDatum(const Date& d, const std::string& name) {
    return QuantLib::ext::make_shared<MarketDatum>(0.0, d, name, MarketDatum::QuoteType::NONE,
                                           MarketDatum::InstrumentType::NONE);
}
} // namespace

std::vector<QuantLib::ext::shared_ptr<MarketDatum>> InMemoryLoader::loadQuotes(const QuantLib::Date& d) const {
    auto it = data_.find(d);
    if(it == data_.end())
	return {};
    return std::vector<QuantLib::ext::shared_ptr<MarketDatum>>(it->second.begin(), it->second.end());
}

QuantLib::ext::shared_ptr<MarketDatum> InMemoryLoader::get(const string& name, const QuantLib::Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "No datum for " << name << " on date " << d);
    auto it2 = it->second.find(makeDummyMarketDatum(d, name));
    QL_REQUIRE(it2 != it->second.end(), "No datum for " << name << " on date " << d);
    return *it2;
}

std::set<QuantLib::ext::shared_ptr<MarketDatum>> InMemoryLoader::get(const std::set<std::string>& names,
                                                             const QuantLib::Date& asof) const {
    auto it = data_.find(asof);
    if(it == data_.end())
        return {};
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> result;
    for (auto const& n : names) {
        auto it2 = it->second.find(makeDummyMarketDatum(asof, n));
        if (it2 != it->second.end())
            result.insert(*it2);
    }
    return result;
}

std::set<QuantLib::ext::shared_ptr<MarketDatum>> InMemoryLoader::get(const Wildcard& wildcard,
                                                             const QuantLib::Date& asof) const {
    if (!wildcard.hasWildcard()) {
        // no wildcard => use get by name function
        try {
            return {get(wildcard.pattern(), asof)};
        } catch (...) {
        }
        return {};
    }
    auto it = data_.find(asof);
    if (it == data_.end())
        return {};
    std::set<QuantLib::ext::shared_ptr<MarketDatum>> result;
    std::set<QuantLib::ext::shared_ptr<MarketDatum>>::iterator it1, it2;
    if (wildcard.wildcardPos() == 0) {
        // wildcard at first position => we have to search all of the data
        it1 = it->second.begin();
        it2 = it->second.end();
    } else {
        // search the range matching the substring of the pattern until the wildcard
        std::string prefix = wildcard.pattern().substr(0, wildcard.wildcardPos());
        it1 = it->second.lower_bound(makeDummyMarketDatum(asof, prefix));
        it2 = it->second.upper_bound(makeDummyMarketDatum(asof, prefix + "\xFF"));
    }
    for (auto it = it1; it != it2; ++it) {
        if (wildcard.isPrefix() || wildcard.matches((*it)->name()))
            result.insert(*it);
    }
    return result;
}

bool InMemoryLoader::hasQuotes(const QuantLib::Date& d) const {
    auto it = data_.find(d);
    return it != data_.end();
}

void InMemoryLoader::add(QuantLib::Date date, const string& name, QuantLib::Real value) {
    QuantLib::ext::shared_ptr<MarketDatum> md;
    try {
        md = parseMarketDatum(date, name, value);
    } catch (std::exception& e) {
        WLOG("Failed to parse MarketDatum " << name << ": " << e.what());
    }
    if (md != nullptr) {
        std::pair<bool, string> addFX = {true, ""};
        if (md->instrumentType() == MarketDatum::InstrumentType::FX_SPOT &&
            md->quoteType() == MarketDatum::QuoteType::RATE) {
            addFX = checkFxDuplicate(md, date);
            if (!addFX.second.empty()) {
                auto it = data_[date].find(makeDummyMarketDatum(date, addFX.second));
                TLOG("Replacing MarketDatum " << addFX.second << " with " << name << " due to FX Dominance.");
                if (it != data_[date].end())
					data_[date].erase(it);
			}
		}
        if (addFX.first && data_[date].insert(md).second) {
            TLOG("Added MarketDatum " << name);
        } else if (!addFX.first) {
            WLOG("Skipped MarketDatum " << name << " - dominant FX already present.")
        } else {
            WLOG("Skipped MarketDatum " << name << " - this is already present.");
        }
    }
}

void InMemoryLoader::addFixing(QuantLib::Date date, const string& name, QuantLib::Real value) {
    if (!fixings_.insert(Fixing(date, name, value)).second) {
        WLOG("Skipped Fixing " << name << "@" << QuantLib::io::iso_date(date) << " - this is already present.");
    }
}

void InMemoryLoader::addDividend(const QuantExt::Dividend& dividend) {
    if (!dividends_.insert(dividend).second) {
        WLOG("Skipped Dividend " << dividend.name << "@" << QuantLib::io::iso_date(dividend.exDate) << " - this is already present.");
    }
}

void InMemoryLoader::reset() {
    data_.clear();
    fixings_.clear();
    dividends_.clear();
    actualDate_ = Date();
}

void load(InMemoryLoader& loader, const vector<string>& data, bool isMarket, bool implyTodaysFixings) {
    LOG("MemoryLoader started");

    Date today = QuantLib::Settings::instance().evaluationDate();

    for (Size i = 0; i < data.size(); ++i) {
        string line = data[i];
        // skip blank and comment lines
        if (line.size() > 0 && line[0] != '#') {
            vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_on);

            // TODO: should we try, catch and log any invalid lines?
            QL_REQUIRE(tokens.size() == 3, "Invalid MemoryLoader line, 3 tokens expected " << line);
            Date date = parseDate(tokens[0]);
            const string& key = tokens[1];
            Real value = parseReal(tokens[2]);

            if (isMarket) {
                // process market
                // build market datum and add to map
                try {
                    loader.add(date, key, value);
                    TLOG("Added MarketDatum " << key);
                } catch (std::exception& e) {
                    WLOG("Failed to parse MarketDatum " << key << ": " << e.what());
                }
            } else {
                // process fixings
                if (date < today || (date == today && !implyTodaysFixings))
                    loader.addFixing(date, key, value);
            }
        }
    }
    LOG("MemoryLoader completed");
}

void loadDataFromBuffers(InMemoryLoader& loader, const std::vector<std::string>& marketData,
                         const std::vector<std::string>& fixingData, bool implyTodaysFixings) {
    load(loader, marketData, true, implyTodaysFixings);
    load(loader, fixingData, false, implyTodaysFixings);
}

} // namespace data
} // namespace ore
