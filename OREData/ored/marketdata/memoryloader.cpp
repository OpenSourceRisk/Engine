/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
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

/*! \file aarealdata/marketdata/memoryloader.cpp
    \brief Market Datum Loader impl
    \ingroup
*/

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <map>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ored/marketdata/memoryloader.hpp>

using namespace std;

namespace ore {
namespace data {

MemoryLoader::MemoryLoader(const vector<string>& marketData, const vector<string>& fixingData, bool implyTodaysFixings)
    : implyTodaysFixings_(implyTodaysFixings) {

    // load market data
    load(marketData, true);
    // log
    for (auto it : data_) {
        LOG("MemoryLoader loaded " << it.second.size() << " market data points for " << it.first);
    }

    // load fixings
    load(fixingData, false);
    LOG("MemoryLoader loaded " << fixings_.size() << " fixings");

    LOG("MemoryLoader complete.");
}

void MemoryLoader::load(const vector<string>& data, bool isMarket) {
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
                    data_[date].push_back(parseMarketDatum(date, key, value));
                    TLOG("Added MarketDatum " << data_[date].back()->name());
                } catch (std::exception& e) {
                    WLOG("Failed to parse MarketDatum " << key << ": " << e.what());
                }
            } else {
                // process fixings
                if (date < today || (date == today && !implyTodaysFixings_))
                    fixings_.emplace_back(Fixing(date, key, value));
            }
        }
    }
    LOG("MemoryLoader completed");
}

const vector<boost::shared_ptr<MarketDatum>>& MemoryLoader::loadQuotes(const QuantLib::Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "MemoryLoader has no data for date " << d);
    return it->second;
}

const boost::shared_ptr<MarketDatum>& MemoryLoader::get(const string& name, const QuantLib::Date& d) const {
    for (auto& md : loadQuotes(d)) {
        if (md->name() == name)
            return md;
    }
    QL_FAIL("No MarketDatum for name " << name << " and date " << d);
}
} // namespace data
} // namespace ore
