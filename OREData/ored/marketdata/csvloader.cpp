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

/*! \file ored/marketdata/csvloader.cpp
    \brief Market Datum Loader impl
    \ingroup
*/

#include <boost/algorithm/string.hpp>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/log.hpp>
#include <fstream>
#include <algorithm>
#include <map>

using namespace std;

namespace ore {
namespace data {

CSVLoader::CSVLoader(const string& marketFilename, const string& fixingFilename, bool implyTodaysFixings)
    : implyTodaysFixings_(implyTodaysFixings) {

    // load market data
    loadFile(marketFilename, true);
    // log
    for (auto it : data_) {
        LOG("CSVLoader loaded " << it.second.size() << " market data points for " << it.first);
    }

    // load fixings
    loadFile(fixingFilename, false);
    LOG("CSVLoader loaded " << fixings_.size() << " fixings");

    LOG("CSVLoader complete.");
}

void CSVLoader::loadFile(const string& filename, bool isMarket) {
    LOG("CSVLoader loading from " << filename);

    Date today = QuantLib::Settings::instance().evaluationDate();

    ifstream file;
    file.open(filename.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << filename);

    while (!file.eof()) {
        string line;
        getline(file, line);
        // skip blank and comment lines
        if (line.size() > 0 && line[0] != '#') {

            vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_on);

            // TODO: should we try, catch and log any invalid lines?
            QL_REQUIRE(tokens.size() == 3, "Invalid CSVLoader line, 3 tokens expected " << line);
            Date date = parseDate(tokens[0]);
            const string& key = tokens[1];
            Real value = parseReal(tokens[2]);

            if (isMarket) {
                // process market
                // build market datum and add to map
                try {
                    data_[date].push_back(parseMarketDatum(date, key, value));
                    DLOG("Added MarketDatum " << data_[date].back()->name());
                } catch (std::exception& e) {
                    LOG("Failed to parse MarketDatum " << key << ": " << e.what());
                }
            } else {
                // process fixings
                if (date < today || (date == today && !implyTodaysFixings_))
                    fixings_.emplace_back(Fixing(date, key, value));
            }
        }
    }
    file.close();
    LOG("CSVLoader completed processing " << filename);
}

const vector<boost::shared_ptr<MarketDatum>>& CSVLoader::loadQuotes(const QuantLib::Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "CSVLoader has no data for date " << d);
    return it->second;
}

const boost::shared_ptr<MarketDatum>& CSVLoader::get(const string& name, const QuantLib::Date& d) const {
    for (auto& md : loadQuotes(d)) {
        if (md->name() == name)
            return md;
    }
    QL_FAIL("No MarketDatum for name " << name << " and date " << d);
}
}
}
