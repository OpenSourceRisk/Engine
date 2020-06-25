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

#include <orea/cube/cubewriter.hpp>
#include <ostream>
#include <ql/errors.hpp>
#include <stdio.h>

using QuantLib::Date;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

CubeWriter::CubeWriter(const std::string& filename) : filename_(filename) {}

void CubeWriter::write(const boost::shared_ptr<NPVCube>& cube, const std::map<std::string, std::string>& nettingSetMap,
                       bool append) {

    // Convert dates into strings
    vector<string> dateStrings(cube->numDates());
    for (Size i = 0; i < cube->numDates(); ++i) {
        std::ostringstream oss;
        oss << QuantLib::io::iso_date(cube->dates()[i]);
        dateStrings[i] = oss.str();
    }
    std::ostringstream oss;
    oss << QuantLib::io::iso_date(cube->asof());
    string asofString = oss.str();

    const vector<string>& ids = cube->ids();

    FILE* fp = fopen(filename_.c_str(), append ? "a" : "w");
    QL_REQUIRE(fp, "error opening file " << filename_);
    if (!append)
        fprintf(fp, "Id,NettingSet,DateIndex,Date,Sample,Depth,Value\n");
    const char* fmt = "%s,%s,%lu,%s,%lu,%lu,%.4f\n";

    // Get netting Set Ids (or "" if not there)
    vector<const char*> nettingSetIds(ids.size());
    // T0
    for (Size i = 0; i < ids.size(); i++) {
        if (nettingSetMap.find(ids[i]) != nettingSetMap.end())
            nettingSetIds[i] = nettingSetMap.at(ids[i]).c_str();
        else
            nettingSetIds[i] = "";

        fprintf(fp, fmt, ids[i].c_str(), nettingSetIds[i], static_cast<Size>(0), asofString.c_str(),
                static_cast<Size>(0), static_cast<Size>(0), cube->getT0(i));
    }
    // Cube
    for (Size i = 0; i < ids.size(); i++) {
        for (Size j = 0; j < cube->numDates(); j++) {
            for (Size k = 0; k < cube->samples(); k++) {
                for (Size l = 0; l < cube->depth(); l++) {
                    fprintf(fp, fmt, ids[i].c_str(), nettingSetIds[i], j + 1, dateStrings[j].c_str(), k + 1, l,
                            cube->get(i, j, k, l));
                }
            }
        }
    }
    fclose(fp);
}
} // namespace analytics
} // namespace ore
