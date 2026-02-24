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

#include <filesystem>
#include <boost/iostreams/device/file_descriptor.hpp>
#ifdef ORE_USE_ZLIB
#include <boost/iostreams/filter/gzip.hpp>
#endif
#include <boost/iostreams/filtering_stream.hpp>

#include <charconv>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <regex>

namespace ore {
namespace analytics {

namespace {

// Minimal benchmark timer — remove after profiling
struct ScopedTimer {
    const char* label;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    ~ScopedTimer() {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        std::cout << "[CubeIO] " << label << ": " << ms << " ms\n";
    }
};

// ---------------------------------------------------------------------------
// Zero-allocation helpers that walk a raw char pointer and parse comma-
// separated unsigned integers / doubles without any heap allocation.
// After a successful parse `p` is advanced past the parsed token AND
// the following comma (if present).  Returns true on success.
// ---------------------------------------------------------------------------
inline bool fast_parse_size(const char*& p, const char* end, Size& out) {
    auto [ptr, ec] = std::from_chars(p, end, out);
    if (ec != std::errc{})
        return false;
    p = ptr;
    if (p != end && *p == ',')
        ++p;
    return true;
}

inline bool fast_parse_double(const char*& p, const char* end, double& out) {
    auto [ptr, ec] = std::from_chars(p, end, out);
    if (ec != std::errc{})
        return false;
    p = ptr;
    if (p != end && *p == ',')
        ++p;
    return true;
}

// ---------------------------------------------------------------------------
// Buffered writer — batches small writes into a 1 MB buffer before flushing
// to the underlying filtering_stream, reducing per-write overhead through the
// gzip compressor filter chain (B2).
// ---------------------------------------------------------------------------
struct BufWriter {
    static constexpr std::size_t CAPACITY = 1u << 20; // 1 MB
    boost::iostreams::filtering_stream<boost::iostreams::output>& sink_;
    std::vector<char> buf_;
    std::size_t pos_ = 0;

    explicit BufWriter(boost::iostreams::filtering_stream<boost::iostreams::output>& s)
        : sink_(s), buf_(CAPACITY) {}

    void flush() {
        if (pos_ > 0) {
            sink_.write(buf_.data(), static_cast<std::streamsize>(pos_));
            pos_ = 0;
        }
    }

    // n must be < CAPACITY (guaranteed: max line length here is ~128 bytes)
    void write(const char* data, std::size_t n) {
        if (pos_ + n > CAPACITY - 512)
            flush();
        std::memcpy(buf_.data() + pos_, data, n);
        pos_ += n;
    }

    void put(char c) {
        if (pos_ + 1 > CAPACITY - 512)
            flush();
        buf_[pos_++] = c;
    }

    ~BufWriter() { flush(); }
};

bool use_compression(const std::string& filename) {
#ifdef ORE_USE_ZLIB
    // assume compression for all filenames that do not end with csv or txt

    std::string extension = std::filesystem::path(filename).extension().string();
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

QuantLib::ext::shared_ptr<NPVCubeWithMetaData> loadCube(const std::string& filename) {
    ScopedTimer _t{"loadCube"};

    auto result = QuantLib::ext::make_shared<NPVCubeWithMetaData>();

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
    bool useDoublePrecision = ore::data::parseBool(getMetaData(line, "usesDblPrc"));

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
        auto sgd = QuantLib::ext::make_shared<ScenarioGeneratorData>();
        sgd->fromXMLString(md);
        result->setScenarioGeneratorData(sgd);
        std::getline(in, line);
        DLOG("overwrite scenario generator data with meta data from cube: " << md);
    }

    if (std::string md = getMetaData(line, "storeFlows", false); !md.empty()) {
        result->setStoreFlows(parseBool(md));
        std::getline(in, line);
        DLOG("overwrite storeFlows with meta data from cube: " << md);
    }

    if (std::string md = getMetaData(line, "storeCrSt", false); !md.empty()) {
        result->storeCreditStateNPVs(parseInteger(md));
        std::getline(in, line);
        DLOG("overwrite storeCreditStateNPVs with meta data from cube: " << md);
    }

    QuantLib::ext::shared_ptr<NPVCube> cube;
    if (useDoublePrecision) {
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<double>>(asof, ids, dates, samples, depth, 0.0);
    } else {
        cube = QuantLib::ext::make_shared<InMemoryCubeOpt<float>>(asof, ids, dates, samples, depth, 0.0);
    }
    result->setCube(cube);

    Size nData = 0;
    while (!in.eof()) {
        std::getline(in, line);
        if (line.empty())
            continue;
        // Zero-allocation fast parse: walk the raw string without splitting
        const char* p   = line.data();
        const char* end = p + line.size();
        Size id = 0, date = 0, sample = 0, depth = 0;
        double value = 0.0;
        QL_REQUIRE(fast_parse_size(p, end, id) && fast_parse_size(p, end, date) &&
                       fast_parse_size(p, end, sample) && fast_parse_size(p, end, depth) &&
                       fast_parse_double(p, end, value),
                   "loadCube(): invalid data line '" << line << "'");
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

void saveCube(const std::string& filename, const NPVCubeWithMetaData& cube) {
    ScopedTimer _t{"saveCube"};

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

    out << "# asof       : " << ore::data::to_string(cube.cube()->asof()) << "\n";
    out << "# numIds     : " << std::to_string(cube.cube()->numIds()) << "\n";
    out << "# numDates   : " << std::to_string(cube.cube()->numDates()) << "\n";
    out << "# samples    : " << ore::data::to_string(cube.cube()->samples()) << "\n";
    out << "# depth      : " << ore::data::to_string(cube.cube()->depth()) << "\n";
    out << "# usesDblPrc : " << std::boolalpha << cube.cube()->usesDoublePrecision() << "\n";
    out << "# dates      : \n";
    for (auto const& d : cube.cube()->dates())
        out << "# " << ore::data::to_string(d) << "\n";

    out << "# ids        : \n";
    std::map<Size, std::string> ids;
    for (auto const& d : cube.cube()->idsAndIndexes()) {
        ids[d.second] = d.first;
    }
    for (auto const& d : ids) {
        out << "# " << d.second << "\n";
    }

    if (cube.scenarioGeneratorData()) {
        std::string scenGenDataXml =
            std::regex_replace(cube.scenarioGeneratorData()->toXMLString(), std::regex("\\r\\n|\\r|\\n|\\t"), "");
        out << "# scenGenDta : " << scenGenDataXml << "\n";
    }
    if (cube.storeFlows()) {
        out << "# storeFlows : " << std::boolalpha << *cube.storeFlows() << "\n";
    }
    if (cube.storeCreditStateNPVs()) {
        out << "# storeCrSt  : " << *cube.storeCreditStateNPVs() << "\n";
    }

    // write cube data — buffered to amortise per-write overhead of the
    // gzip filter chain (B2); charconv for locale-independent formatting (P5).
    {
        BufWriter w(out);
        static constexpr char hdr[] = "#id,date,sample,depth,value\n";
        w.write(hdr, sizeof(hdr) - 1);

        char idxBuf[128]; // sufficient for 4 x uint64 indices + commas
        char valBuf[32];  // sufficient for any double in shortest round-trip form

        for (Size i = 0; i < cube.cube()->numIds(); ++i) {
            // T0 line: "i,0,0,0,value\n"
            {
                char* p = std::to_chars(idxBuf, idxBuf + 24, i).ptr;
                *p++ = ','; *p++ = '0'; *p++ = ','; *p++ = '0'; *p++ = ','; *p++ = '0'; *p++ = ',';
                auto [vp, vec] = std::to_chars(valBuf, valBuf + sizeof(valBuf), cube.cube()->getT0(i));
                w.write(idxBuf, static_cast<std::size_t>(p - idxBuf));
                w.write(valBuf, static_cast<std::size_t>(vp - valBuf));
                w.put('\n');
            }

            char* p0_end = std::to_chars(idxBuf, idxBuf + 24, i).ptr;
            *p0_end++ = ',';
            for (Size j = 0; j < cube.cube()->numDates(); ++j) {
                char* p1_end = std::to_chars(p0_end, p0_end + 24, j + 1).ptr;
                *p1_end++ = ',';
                for (Size k = 0; k < cube.cube()->samples(); ++k) {
                    char* p2_end = std::to_chars(p1_end, p1_end + 24, k).ptr;
                    *p2_end++ = ',';
                    for (Size d = 0; d < cube.cube()->depth(); ++d) {
                        double value = cube.cube()->get(i, j, k, d);
                        if (value != 0.0) {
                            char* p3_end = std::to_chars(p2_end, p2_end + 24, d).ptr;
                            *p3_end++ = ',';
                            auto [vp, vec] = std::to_chars(valBuf, valBuf + sizeof(valBuf), value);
                            w.write(idxBuf, static_cast<std::size_t>(p3_end - idxBuf));
                            w.write(valBuf, static_cast<std::size_t>(vp - valBuf));
                            w.put('\n');
                        }
                    }
                }
            }
        }
    } // BufWriter flushes on destruction
}

QuantLib::ext::shared_ptr<AggregationScenarioData> loadAggregationScenarioData(const std::string& filename) {
    ScopedTimer _t{"loadAggScenData"};

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

    std::getline(in, line);
    Size numKeys = ore::data::parseInteger(getMetaData(line, "keys"));
    std::vector<std::pair<AggregationScenarioDataType, std::string>> keys;
    for (Size i = 0; i < numKeys; ++i) {
        std::getline(in, line);
        // Format: "# <keyType>,<keyName>"
        QL_REQUIRE(line.size() >= 2 && line[0] == '#' && line[1] == ' ',
                   "loadAggregationScenarioData(): invalid key line '" << line << "'");
        const char* p   = line.data() + 2;
        const char* end = line.data() + line.size();
        Size keyType = 0;
        QL_REQUIRE(fast_parse_size(p, end, keyType),
                   "loadAggregationScenarioData(): invalid key type in line '" << line << "'");
        keys.push_back(std::make_pair(AggregationScenarioDataType(keyType), std::string(p, end)));
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
        // Zero-allocation fast parse
        const char* p   = line.data();
        const char* end = p + line.size();
        Size date = 0, sample = 0, key = 0;
        double value = 0.0;
        QL_REQUIRE(fast_parse_size(p, end, date) && fast_parse_size(p, end, sample) &&
                       fast_parse_size(p, end, key) && fast_parse_double(p, end, value),
                   "loadAggregationScenarioData(): invalid data line '" << line << "'");
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
    ScopedTimer _t{"saveAggScenData"};

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

    // write data — buffered (B2); all fields formatted via charconv (P5) so
    // operator<< is never called in the hot loop.
    {
        BufWriter w(out);
        static constexpr char hdr[] = "#date,sample,key,value\n";
        w.write(hdr, sizeof(hdr) - 1);

        char lineBuf[128]; // date + sample + key + value + 3 commas + newline
        for (Size i = 0; i < cube.dimDates(); ++i) {
            for (Size j = 0; j < cube.dimSamples(); ++j) {
                for (Size k = 0; k < keys.size(); ++k) {
                    char* p = lineBuf;
                    p = std::to_chars(p, p + 20, i + 1).ptr; *p++ = ',';
                    p = std::to_chars(p, p + 20, j).ptr;     *p++ = ',';
                    p = std::to_chars(p, p + 20, k).ptr;     *p++ = ',';
                    p = std::to_chars(p, p + 32,
                                      cube.get(i, j, keys[k].first, keys[k].second)).ptr;
                    *p++ = '\n';
                    w.write(lineBuf, static_cast<std::size_t>(p - lineBuf));
                }
            }
        }
    } // BufWriter flushes on destruction
}

} // namespace analytics
} // namespace ore
