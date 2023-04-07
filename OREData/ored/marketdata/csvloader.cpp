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

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <fstream>
#include <map>
#include <ored/marketdata/csvloader.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace std;

namespace ore {
namespace data {

CSVLoader::CSVLoader(const string& marketFilename, const string& fixingFilename, bool implyTodaysFixings)
    : CSVLoader(marketFilename, fixingFilename, "", implyTodaysFixings) {}

CSVLoader::CSVLoader(const vector<string>& marketFiles, const vector<string>& fixingFiles, bool implyTodaysFixings)
    : CSVLoader(marketFiles, fixingFiles, {}, implyTodaysFixings) {}

CSVLoader::CSVLoader(const string& marketFilename, const string& fixingFilename, const string& dividendFilename,
                     bool implyTodaysFixings)
    : implyTodaysFixings_(implyTodaysFixings) {

    // load market data
    loadFile(marketFilename, DataType::Market);
    // log
    for (auto it : data_) {
        LOG("CSVLoader loaded " << it.second.size() << " market data points for " << it.first);
    }

    // load fixings
    loadFile(fixingFilename, DataType::Fixing);
    LOG("CSVLoader loaded " << fixings_.size() << " fixings");

    // load dividends
    if (dividendFilename != "") {
        loadFile(dividendFilename, DataType::Dividend);
        LOG("CSVLoader loaded " << dividends_.size() << " dividends");
    }

    LOG("CSVLoader complete.");
}

CSVLoader::CSVLoader(const vector<string>& marketFiles, const vector<string>& fixingFiles,
                     const vector<string>& dividendFiles, bool implyTodaysFixings)
    : implyTodaysFixings_(implyTodaysFixings) {

    for (auto marketFile : marketFiles)
        // load market data
        loadFile(marketFile, DataType::Market);

    // log
    for (auto it : data_)
        LOG("CSVLoader loaded " << it.second.size() << " market data points for " << it.first);

    for (auto fixingFile : fixingFiles)
        // load fixings
        loadFile(fixingFile, DataType::Fixing);
    LOG("CSVLoader loaded " << fixings_.size() << " fixings");

    for (auto dividendFile : dividendFiles)
        // load dividends
        loadFile(dividendFile, DataType::Dividend);
    LOG("CSVLoader loaded " << dividends_.size() << " dividends");

    LOG("CSVLoader complete.");
}

void CSVLoader::loadFile(const string& filename, DataType dataType) {
    LOG("CSVLoader loading from " << filename);

    Date today = QuantLib::Settings::instance().evaluationDate();

    ifstream file;
    file.open(filename.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << filename);

    while (!file.eof()) {
        string line;
        getline(file, line);
        boost::trim(line);
        // skip blank and comment lines
        if (line.size() > 0 && line[0] != '#') {

            vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_on);

            // TODO: should we try, catch and log any invalid lines?
            QL_REQUIRE(tokens.size() == 3 || tokens.size() == 4, "Invalid CSVLoader line, 3 tokens expected " << line);
            if (tokens.size() == 4)
                QL_REQUIRE(dataType == DataType::Dividend, "CSVLoader, dataType must be of type Dividend");
            Date date = parseDate(tokens[0]);
            const string& key = tokens[1];
            Real value = parseReal(tokens[2]);

            if (dataType == DataType::Market) {
                // process market
                // build market datum and add to map
                try {
                    boost::shared_ptr<MarketDatum> md;
                    try {
                        md = parseMarketDatum(date, key, value);
                    } catch (std::exception& e) {
                        WLOG("Failed to parse MarketDatum " << key << ": " << e.what());
                    }
                    if (md != nullptr) {
                        if (data_[date].insert(md).second) {
                            TLOG("Added MarketDatum " << key);
                        } else {
                            WLOG("Skipped MarketDatum " << key << " - this is already present.");
                        }
                    }
                } catch (std::exception& e) {
                    WLOG("Failed to parse MarketDatum " << key << ": " << e.what());
                }
            } else if (dataType == DataType::Fixing) {
                // process fixings
                if (date < today || (date == today && !implyTodaysFixings_)) {
                    if(!fixings_.insert(Fixing(date, key, value)).second) {
                        WLOG("Skipped Fixing " << key << "@" << QuantLib::io::iso_date(date)
                                               << " - this is already present.");
                    }
                }
            } else if (dataType == DataType::Dividend) {
                Date payDate = date;
                if (tokens.size() == 4)
                    payDate = parseDate(tokens[3]);
                // process dividends
                if (date <= today) {
                    if (!dividends_.insert(QuantExt::Dividend(date, key, value, payDate)).second) {
                        WLOG("Skipped Dividend " << key << "@" << QuantLib::io::iso_date(date)
                                                 << " - this is already present.");
                    }
                }
            } else {
                QL_FAIL("unknown data type");
            }
        }
    }
    file.close();
    LOG("CSVLoader completed processing " << filename);
}

vector<boost::shared_ptr<MarketDatum>> CSVLoader::loadQuotes(const QuantLib::Date& d) const {
    auto it = data_.find(d);
    if (it == data_.end())
        return {};
    return std::vector<boost::shared_ptr<MarketDatum>>(it->second.begin(), it->second.end());
}

boost::shared_ptr<MarketDatum> makeDummyMarketDatum(const Date& d, const std::string& name) {
    return boost::make_shared<MarketDatum>(0.0, d, name, MarketDatum::QuoteType::NONE,
                                           MarketDatum::InstrumentType::NONE);
}


boost::shared_ptr<MarketDatum> CSVLoader::get(const string& name, const QuantLib::Date& d) const {
    auto it = data_.find(d);
    QL_REQUIRE(it != data_.end(), "No datum for " << name << " on date " << d);
    auto it2 = it->second.find(makeDummyMarketDatum(d, name));
    QL_REQUIRE(it2 != it->second.end(), "No datum for " << name << " on date " << d);
    return *it2;
}

std::set<boost::shared_ptr<MarketDatum>> CSVLoader::get(const std::set<std::string>& names,
                                                             const QuantLib::Date& asof) const {
    auto it = data_.find(asof);
    if(it == data_.end())
        return {};
    std::set<boost::shared_ptr<MarketDatum>> result;
    for (auto const& n : names) {
        auto it2 = it->second.find(makeDummyMarketDatum(asof, n));
        if (it2 != it->second.end())
            result.insert(*it2);
    }
    return result;
}

std::set<boost::shared_ptr<MarketDatum>> CSVLoader::get(const Wildcard& wildcard,
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
    std::set<boost::shared_ptr<MarketDatum>> result;
    std::set<boost::shared_ptr<MarketDatum>>::iterator it1, it2;
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
} // namespace data
} // namespace ore
