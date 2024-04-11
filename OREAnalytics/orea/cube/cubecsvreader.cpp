/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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
#include <fstream>
#include <orea/cube/inmemorycube.hpp>
#include <orea/cube/cubecsvreader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>
#include <stdio.h>

using QuantLib::Date;
using std::string;
using std::vector;
using namespace ore::analytics;

namespace ore {
namespace analytics {

CubeCsvReader::CubeCsvReader(const std::string& filename) : filename_(filename) {}

void CubeCsvReader::read(QuantLib::ext::shared_ptr<NPVCube>& cube, std::map<std::string, std::string>& nettingSetMap) {

    std::set<string> tradeIds;
    std::vector<Date> dateVec;
    std::set<Size> sampleIdxSet, depthIdxSet;
    Date asof;

    std::ifstream file;
    file.open(filename_.c_str());
    QL_REQUIRE(file.is_open(), "error opening file " << filename_);
    bool headerAlreadyLoaded1 = false;
    while (!file.eof()) {
        string line;
        getline(file, line);
        // skip blank and comment lines
        if (line.size() > 0 && line[0] != '#') {
            if (!headerAlreadyLoaded1) {
                headerAlreadyLoaded1 = true;
                continue;
            }
            vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t"), boost::token_compress_on);
            QL_REQUIRE(tokens.size() == 7, "Invalid CubeCsvReader line, 7 tokens expected " << line);
            string tradeId = tokens[0];
            string nettingId = tokens[1];
            Size dateIdx = ore::data::parseInteger(tokens[2]);
            Date gridDate = ore::data::parseDate(tokens[3]);
            Size sampleIdx = ore::data::parseInteger(tokens[4]);
            Size depthIdx = ore::data::parseInteger(tokens[5]);

            if (dateIdx == 0) {
                asof = gridDate;
            } else if (std::find(dateVec.begin(), dateVec.end(), gridDate) == dateVec.end()) {
                dateVec.push_back(gridDate);
            }
            tradeIds.insert(tradeId);
            if (nettingSetMap.find(tradeId) == nettingSetMap.end())
                nettingSetMap[tradeId] = nettingId;
            sampleIdxSet.insert(sampleIdx);
            depthIdxSet.insert(depthIdx);
        }
    }
    file.close();

    QL_REQUIRE(dateVec.size() > 0, "CubeCsvReader - no simulation dates found");
    QL_REQUIRE(tradeIds.size() > 0, "CubeCsvReader - no trades found");
    Size numSamples = sampleIdxSet.size() - 1; // zero represents t0 data
    Size cubeDepth = depthIdxSet.size();
    QL_REQUIRE(numSamples > 0, "CubeCsvReader - no simulation paths found");
    QL_REQUIRE(cubeDepth > 0, "CubeCsvReader - no cube depth");
    QL_REQUIRE(nettingSetMap.size() == tradeIds.size(),
               "CubeCsvReader - vector size mismatch - trade Ids vs netting map");
    for (Size i = 0; i < dateVec.size(); ++i) {
        QL_REQUIRE(dateVec[i] > asof, "CubeCsvReader - grid date " << QuantLib::io::iso_date(dateVec[i])
                                                                   << "must be greater than asof "
                                                                   << QuantLib::io::iso_date(asof));
        if (i > 0) {
            QL_REQUIRE(dateVec[i] > dateVec[i - 1], "CubeCsvReader - date grid must be monotonic increasing");
        }
    }
    if (cubeDepth == 1)
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, tradeIds, dateVec, numSamples);
    else if (cubeDepth > 1)
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(asof, tradeIds, dateVec, numSamples, cubeDepth);

    // Now re-open the file and loop through its contents AGAIN
    std::ifstream file2;
    file2.open(filename_.c_str());
    QL_REQUIRE(file2.is_open(), "error opening file " << filename_);
    bool headerAlreadyLoaded = false;
    while (!file2.eof()) {
        string line;
        getline(file2, line);
        // skip blank and comment lines
        if (line.size() > 0 && line[0] != '#') {
            if (!headerAlreadyLoaded) {
                headerAlreadyLoaded = true;
                continue;
            }
            vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t"), boost::token_compress_on);
            QL_REQUIRE(tokens.size() == 7, "Invalid CubeCsvReader line, 7 tokens expected " << line);
            string tradeId = tokens[0];
            string nettingId = tokens[1];
            Size dateIdx = ore::data::parseInteger(tokens[2]);
            // Date gridDate = ore::data::parseDate(tokens[3]); Not needed
            Size sampleIdx = ore::data::parseInteger(tokens[4]);
            Size depthIdx = ore::data::parseInteger(tokens[5]);
            Real value = ore::data::parseReal(tokens[6]);

            auto pos_trade = cube->getTradeIndex(tradeId);

            if (dateIdx == 0) {
                cube->setT0(value, pos_trade, depthIdx);
            } else {
                QL_REQUIRE(sampleIdx > 0, "CubeCsvReader - input sampleIdx should be > 0");
                cube->set(value, pos_trade, dateIdx - 1, sampleIdx - 1, depthIdx);
            }
        }
    }
    file2.close();
}
} // namespace analytics
} // namespace ore
