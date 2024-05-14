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

#include <orea/engine/sensitivityfilestream.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>

using ore::analytics::deconstructFactor;
using ore::data::parseBool;
using ore::data::parseReal;
using std::getline;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

void SensitivityInputStream::setStream(std::istream* stream) { 
    stream_ = stream; 
}

SensitivityRecord SensitivityInputStream::next() {
    // Get the next valid SensitivityRecord
    string line;
    while (getline(*stream_, line)) {
        // Update the current line number
        ++lineNo_;

        // Trim leading and trailing space
        boost::trim(line);

        // If line is empty or a comment line, skip to next
        if (line.empty() || boost::starts_with(line, comment_))
            continue;

        // Try to parse line in to a SensitivityRecord
        DLOG("Processing line number " << lineNo_ << ": " << line);
        vector<string> entries;
        boost::split(
            entries, line, [this](char c) { return c == delim_; }, boost::token_compress_off);
        return processRecord(entries);
    }

    // If we get to here, no more lines to process so return empty record
    return SensitivityRecord();
}

void SensitivityInputStream::reset() {
    // Reset to beginning of file and line number
    stream_->clear();
    stream_->seekg(0, std::ios::beg);
    lineNo_ = 0;
}

SensitivityRecord SensitivityInputStream::processRecord(const vector<string>& entries) const {

    QL_REQUIRE(entries.size() == 10, "On line number " << lineNo_ << ": A sensitivity record needs 10 entries");

    SensitivityRecord sr;
    sr.tradeId = entries[0];
    sr.isPar = parseBool(entries[1]);

    auto p = deconstructFactor(entries[2]);
    sr.key_1 = p.first;
    sr.desc_1 = p.second;
    tryParseReal(entries[3], sr.shift_1);

    p = deconstructFactor(entries[4]);
    sr.key_2 = p.first;
    sr.desc_2 = p.second;
    tryParseReal(entries[5], sr.shift_2);

    sr.currency = entries[6];
    sr.baseNpv = parseReal(entries[7]);
    sr.delta = parseReal(entries[8]);
    tryParseReal(entries[9], sr.gamma); // might be #N/A, if not computed

    return sr;
}

SensitivityFileStream::SensitivityFileStream(const string& fileName, char delim, const string& comment)
    : SensitivityInputStream(delim, comment) {

    // set file name
    file_ = new std::ifstream(fileName);
    QL_REQUIRE(file_->is_open(), "error opening file " << fileName);
    LOG("The file " << fileName << " has been opened for streaming");

    // pass stream to function set stream
    setStream(file_);
}

SensitivityFileStream::~SensitivityFileStream() {
    // Close the file if still open
    if (file_->is_open()) {
        file_->close();
    }
    LOG("The file stream has been closed");
}

SensitivityBufferStream::SensitivityBufferStream(const std::string& buffer, char delim, const std::string& comment)
    : SensitivityInputStream(delim, comment) {
    std::stringstream* stream = new std::stringstream(buffer);
    setStream(stream);
}

} // namespace analytics
} // namespace ore
