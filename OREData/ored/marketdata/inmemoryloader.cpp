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

#include <ored/marketdata/inmemoryloader.hpp>
#include <boost/algorithm/string.hpp> 
#include <ored/marketdata/marketdatumparser.hpp> 
#include <ored/utilities/log.hpp> 
#include <ored/utilities/parsers.hpp> 

using namespace std;
using namespace QuantLib;

namespace ore {
namespace data {

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
    load(loader, marketData, false, implyTodaysFixings);

}

}
}
