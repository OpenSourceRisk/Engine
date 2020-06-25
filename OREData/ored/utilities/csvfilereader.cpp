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

#include <ored/utilities/csvfilereader.hpp>

#include <ql/errors.hpp>
#include <ql/utilities/null.hpp>

#include <boost/algorithm/string.hpp>

using QuantLib::Null;

namespace ore {
namespace data {

CSVFileReader::CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                             const std::string& delimiters, const char eolMarker)
    : fileName_(fileName), hasHeaders_(firstLineContainsHeaders), delimiters_(delimiters), eolMarker_(eolMarker),
      currentLine_(Null<Size>()), numberOfColumns_(Null<Size>()) {
    file_.open(fileName);
    QL_REQUIRE(file_.is_open(), "CSVFileReader: error opening file " << fileName);
    if (firstLineContainsHeaders) {
        QL_REQUIRE(!file_.eof(), "CSVFileReader: file is empty: " << fileName);
        std::string line;
        getline(file_, line, eolMarker);
        boost::trim(line);
        boost::split(headers_, line, boost::is_any_of(delimiters_), boost::token_compress_off);
        numberOfColumns_ = headers_.size();
    }
}

const std::vector<std::string>& CSVFileReader::fields() const {
    QL_REQUIRE(hasHeaders_, "CSVFileReader: no headers were specified for \"" << fileName_ << "\"");
    return headers_;
}

const bool CSVFileReader::hasField(const std::string& field) const {
    return std::find(fields().begin(), fields().end(), field) != fields().end();
}

Size CSVFileReader::numberOfColumns() const {
    QL_REQUIRE(numberOfColumns_ != Null<Size>(), "CSVFileReader: number of columns not known (need call to next())");
    return numberOfColumns_;
}

bool CSVFileReader::next() {
    QL_REQUIRE(file_.is_open(), "CSVFileReader: file is not open, can not move to next line");
    std::string line = "";
    // skip empty lines
    while (line.size() == 0 && !file_.eof()) {
        getline(file_, line, eolMarker_);
        boost::trim(line);
    }
    if (file_.eof() && line.empty()) {
        close();
        return false;
    }
    if (currentLine_ == Null<Size>())
        currentLine_ = 0;
    else
        ++currentLine_;
    boost::split(data_, line, boost::is_any_of(delimiters_), boost::token_compress_off);
    if (numberOfColumns_ == Null<Size>())
        numberOfColumns_ = data_.size();
    else
        QL_REQUIRE(data_.size() == numberOfColumns_, "CSVFileReader: data line #"
                                                         << currentLine_ << " has " << data_.size()
                                                         << " fields, expected " << numberOfColumns_);
    return true;
}

Size CSVFileReader::currentLine() const {
    QL_REQUIRE(currentLine_ != Null<Size>(), "CSVFileReader: current line not known (need call to next())");
    return currentLine_;
}

std::string CSVFileReader::get(const std::string& field) const {
    QL_REQUIRE(hasHeaders_, "CSVFileReader: can not get data by field, file does not have headers");
    QL_REQUIRE(currentLine_ != Null<Size>(), "CSVFileReader: can not get data, need call to next() first");
    Size index = std::find(headers_.begin(), headers_.end(), field) - headers_.begin();
    QL_REQUIRE(index < headers_.size(), "CSVFileReader: field \"" << field << "\" not found.");
    QL_REQUIRE(index < data_.size(), "CSVFileReader: unexpected data size (" << data_.size() << "), required at least "
                                                                             << index + 1 << ", while reading field \""
                                                                             << field << "\"");
    return data_[index];
}

std::string CSVFileReader::get(const Size column) const {
    QL_REQUIRE(column < numberOfColumns_,
               "CSVFileReader: column " << column << " out of bounds 0..." << (numberOfColumns_ - 1));
    QL_REQUIRE(column < data_.size(),
               "CSVFileReader: unexpected data size (" << data_.size() << "), while reading column " << column);
    return data_[column];
}

void CSVFileReader::close() { file_.close(); }

} // namespace data
} // namespace ore
