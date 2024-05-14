/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

#include <orea/cube/cube_io.hpp>
#include <orea/cube/inmemorycube.hpp>

#include <ored/utilities/to_string.hpp>

#include <boost/filesystem.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#ifdef ORE_USE_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#include <boost/iostreams/filtering_stream.hpp>

#include <iomanip>
#include <regex>

namespace ore {
namespace analytics {

namespace {

bool use_compression(const std::string& filename) {
#ifdef ORE_USE_ZLIB
    // assume compression for all filenames that do not end with csv or txt

    std::string extension = boost::filesystem::path(filename).extension().string();
    return extension != ".csv" && extension != ".txt";
#else
    return false;
#endif
}

std::string getMetaData(const std::string& line, const std::string& tag, const bool mandatory = true) {

    // assuming a fixed width format "# tag        : <value>"

    QL_REQUIRE(!mandatory || line.substr(0, 1) == "#",
               "internal error: getMetaData(" << line << ", " << tag << "): line does not start with #");
    QL_REQUIRE(!mandatory || line.substr(2, tag.size()) == tag,
               "internal error: getMetaData(" << line << ", " << tag << ") failed, tag is not matched.");
    return line.substr(0, 1) == "#" && line.substr(2, tag.size()) == tag ? line.substr(15) : std::string();
}

} // namespace

NPVCubeWithMetaData loadCube(const std::string& filename, const bool doublePrecision) {

    NPVCubeWithMetaData result;

    // open file

    bool gzip = use_compression(filename);
    std::ifstream in1(filename, gzip ? (std::ios::binary | std::ios::in) : std::ios::in);
    boost::iostreams::filtering_stream<boost::iostreams::input> in;
#ifdef ORE_USE_ZLIB
    if (gzip)
        in.push(boost::iostreams::gzip_decompressor());
#endif
    in.push(in1);

    // read meta data

    std::string line;

    std::getline(in, line);
    QuantLib::Date asof = ore::data::parseDate(getMetaData(line, "asof"));
    std::getline(in, line);
    Size numIds = ore::data::parseInteger(getMetaData(line, "numIds"));
    std::getline(in, line);
    Size numDates = ore::data::parseInteger(getMetaData(line, "numDates"));
    std::getline(in, line);
    Size samples = ore::data::parseInteger(getMetaData(line, "samples"));
    std::getline(in, line);
    Size depth = ore::data::parseInteger(getMetaData(line, "depth"));

    std::getline(in, line);
    getMetaData(line, "dates");
    std::vector<QuantLib::Date> dates;
    for (Size i = 0; i < numDates; ++i) {
        std::getline(in, line);
        dates.push_back(ore::data::parseDate(line.substr(2)));
    }

    std::getline(in, line);
    getMetaData(line, "ids");
    std::set<std::string> ids;
    for (Size i = 0; i < numIds; ++i) {
        std::getline(in, line);
        ids.insert(line.substr(2));
    }

    std::getline(in, line);
    if (std::string md = getMetaData(line, "scenGenDta", false); !md.empty()) {
        result.scenarioGeneratorData = QuantLib::ext::make_shared<ScenarioGeneratorData>();
        result.scenarioGeneratorData->fromXMLString(md);
        std::getline(in, line);
        DLOG("overwrite scenario generator data with meta data from cube: " << md);
    }

    if (std::string md = getMetaData(line, "storeFlows", false); !md.empty()) {
        result.storeFlows = parseBool(md);
        std::getline(in, line);
        DLOG("overwrite storeFlows with meta data from cube: " << md);
    }

    if (std::string md = getMetaData(line, "storeCrSt", false); !md.empty()) {
        result.storeCreditStateNPVs = parseInteger(md);
        std::getline(in, line);
        DLOG("overwrite storeCreditStateNPVs with meta data from cube: " << md);
    }

    QuantLib::ext::shared_ptr<NPVCube> cube;
    if (doublePrecision && depth <= 1) {
        cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0);
    } else if (doublePrecision && depth > 1) {
        cube = QuantLib::ext::make_shared<DoublePrecisionInMemoryCubeN>(asof, ids, dates, samples, depth, 0.0);
    } else if (!doublePrecision && depth <= 1) {
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCube>(asof, ids, dates, samples, 0.0f);
    } else if (!doublePrecision && depth > 1) {
        cube = QuantLib::ext::make_shared<SinglePrecisionInMemoryCubeN>(asof, ids, dates, samples, depth, 0.0f);
    }
    result.cube = cube;

    vector<string> tokens;
    Size nData = 0;
    while (!in.eof()) {
        std::getline(in, line);
        if (line.empty())
            continue;
        boost::split(tokens, line, [](char c) { return c == ','; });
        QL_REQUIRE(tokens.size() == 5, "loadCube(): invalid data line '" << line << "', expected 5 tokens");
        Size id = ore::data::parseInteger(tokens[0]);
        Size date = ore::data::parseInteger(tokens[1]);
        Size sample = ore::data::parseInteger(tokens[2]);
        Size depth = ore::data::parseInteger(tokens[3]);
        double value = ore::data::parseReal(tokens[4]);
        if (date == 0)
            cube->setT0(value, id, depth);
        else
            cube->set(value, id, date - 1, sample, depth);
        ++nData;
    }

    LOG("loaded cube from " << filename << ": asof = " << asof << ", dim = " << numIds << " x " << numDates << " x "
                            << samples << " x " << depth << ", " << nData << " data lines read.");

    return result;
}

void saveCube(const std::string& filename, const NPVCubeWithMetaData& cube, const bool doublePrecision) {

    // open file

    bool gzip = use_compression(filename);
    std::ofstream out1(filename, gzip ? (std::ios::binary | std::ios::out) : std::ios::out);
    boost::iostreams::filtering_stream<boost::iostreams::output> out;
#ifdef ORE_USE_ZLIB
    if (gzip)
        out.push(boost::iostreams::gzip_compressor(/*boost::iostreams::gzip_params(9)*/));
#endif
    out.push(out1);

    // write meta data (tag width is hardcoded and used in getMetaData())

    out << "# asof       : " << ore::data::to_string(cube.cube->asof()) << "\n";
    out << "# numIds     : " << std::to_string(cube.cube->numIds()) << "\n";
    out << "# numDates   : " << std::to_string(cube.cube->numDates()) << "\n";
    out << "# samples    : " << ore::data::to_string(cube.cube->samples()) << "\n";
    out << "# depth      : " << ore::data::to_string(cube.cube->depth()) << "\n";
    out << "# dates      : \n";
    for (auto const& d : cube.cube->dates())
        out << "# " << ore::data::to_string(d) << "\n";

    out << "# ids        : \n";
    std::map<Size, std::string> ids;
    for (auto const& d : cube.cube->idsAndIndexes()) {
        ids[d.second] = d.first;
    }
    for (auto const& d : ids) {
        out << "# " << d.second << "\n";
    }

    if (cube.scenarioGeneratorData) {
        std::string scenGenDataXml =
            std::regex_replace(cube.scenarioGeneratorData->toXMLString(), std::regex("\\r\\n|\\r|\\n|\\t"), "");
        out << "# scenGenDta : " << scenGenDataXml << "\n";
    }
    if (cube.storeFlows) {
        out << "# storeFlows : " << std::boolalpha << *cube.storeFlows << "\n";
    }
    if (cube.storeCreditStateNPVs) {
        out << "# storeCrSt  : " << *cube.storeCreditStateNPVs << "\n";
    }

    // set precision

    out << std::setprecision(doublePrecision ? std::numeric_limits<double>::max_digits10
                                             : std::numeric_limits<float>::max_digits10);

    // write cube data

    out << "#id,date,sample,depth,value\n";
    for (Size i = 0; i < cube.cube->numIds(); ++i) {
        out << i << ",0,0,0," << cube.cube->getT0(i) << "\n";
        for (Size j = 0; j < cube.cube->numDates(); ++j) {
            for (Size k = 0; k < cube.cube->samples(); ++k) {
                for (Size d = 0; d < cube.cube->depth(); ++d) {
                    double value = cube.cube->get(i, j, k, d);
                    if (value != 0.0)
                        out << i << "," << (j + 1) << "," << k << "," << d << "," << value << "\n";
                }
            }
        }
    }
}

QuantLib::ext::shared_ptr<AggregationScenarioData> loadAggregationScenarioData(const std::string& filename) {

    // open file

    bool gzip = use_compression(filename);
    std::ifstream in1(filename, gzip ? (std::ios::binary | std::ios::in) : std::ios::in);
    boost::iostreams::filtering_stream<boost::iostreams::input> in;
#ifdef ORE_USE_ZLIB
    if (gzip)
        in.push(boost::iostreams::gzip_decompressor());
#endif
    in.push(in1);

    // read meta data

    std::string line;

    std::getline(in, line);
    Size dimDates = ore::data::parseInteger(getMetaData(line, "dimDates"));
    std::getline(in, line);
    Size dimSamples = ore::data::parseInteger(getMetaData(line, "dimSamples"));

    vector<string> tokens;

    std::getline(in, line);
    Size numKeys = ore::data::parseInteger(getMetaData(line, "keys"));
    std::vector<std::pair<AggregationScenarioDataType, std::string>> keys;
    for (Size i = 0; i < numKeys; ++i) {
        std::getline(in, line);
        boost::split(tokens, line.substr(2), [](char c) { return c == ','; });
        QL_REQUIRE(tokens.size() == 2,
                   "loadAggregationScenarioData(): invalid data line '" << line << "', expected 2 tokens");
        keys.push_back(std::make_pair(AggregationScenarioDataType(ore::data::parseInteger(tokens[0])), tokens[1]));
    }

    std::getline(in, line); // header line for data

    QuantLib::ext::shared_ptr<InMemoryAggregationScenarioData> result =
        QuantLib::ext::make_shared<InMemoryAggregationScenarioData>(dimDates, dimSamples);

    std::getline(in, line); // header line for data

    Size nData = 0;
    while (!in.eof()) {
        std::getline(in, line);
        if (line.empty())
            continue;
        boost::split(tokens, line, [](char c) { return c == ','; });
        QL_REQUIRE(tokens.size() == 4,
                   "loadAggregationScenarioData(): invalid data line '" << line << "', expected 4 tokens");
        Size date = ore::data::parseInteger(tokens[0]);
        Size sample = ore::data::parseInteger(tokens[1]);
        Size key = ore::data::parseInteger(tokens[2]);
        double value = ore::data::parseReal(tokens[3]);
        QL_REQUIRE(key < keys.size(), "loadAggregationScenarioData(): invalid data line '" << line << "', key (" << key
                                                                                           << ") is out of range 0..."
                                                                                           << (keys.size() - 1));
        result->set(date - 1, sample, value, keys[key].first, keys[key].second);
        ++nData;
    }

    LOG("loaded aggregation scenario data from " << filename << ": dimDates = " << dimDates
                                                 << ", dimSamples = " << dimSamples << ", keys = " << keys.size()
                                                 << ", " << nData << " data lines read.");

    return result;
}

void saveAggregationScenarioData(const std::string& filename, const AggregationScenarioData& cube) {

    // open file

    bool gzip = use_compression(filename);
    std::ofstream out1(filename, gzip ? (std::ios::binary | std::ios::out) : std::ios::out);
    boost::iostreams::filtering_stream<boost::iostreams::output> out;
#ifdef ORE_USE_ZLIB
    if (gzip)
        out.push(boost::iostreams::gzip_compressor(/*boost::iostreams::gzip_params(9)*/));
#endif
    out.push(out1);

    // write meta data (tag width is hardcoded and used in getMetaData())

    out << "# dimDates   : " << std::to_string(cube.dimDates()) << "\n";
    out << "# dimSamples : " << std::to_string(cube.dimSamples()) << "\n";

    auto keys = cube.keys();

    out << "# keys       : " << std::to_string(keys.size()) << "\n";
    for (auto const& k : keys) {
        out << "# " << (unsigned int)k.first << "," << k.second << "\n";
    }

    // set precision

    out << std::setprecision(std::numeric_limits<double>::max_digits10);

    // write data

    out << "#date,sample,key,value\n";
    for (Size i = 0; i < cube.dimDates(); ++i) {
        for (Size j = 0; j < cube.dimSamples(); ++j) {
            for (Size k = 0; k < keys.size(); ++k) {
                out << (i + 1) << "," << j << "," << k << "," << cube.get(i, j, keys[k].first, keys[k].second) << "\n";
            }
        }
    }
}

} // namespace analytics
} // namespace ore
