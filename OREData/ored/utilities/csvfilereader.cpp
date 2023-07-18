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

#include <ored/utilities/csvfilereader.hpp>

#include <ql/errors.hpp>
#include <ql/utilities/null.hpp>

#include <boost/algorithm/string/trim.hpp>

using QuantLib::Null;

namespace ore {
namespace data {

CSVReader::CSVReader(std::istream* stream, const bool firstLineContainsHeaders, const std::string& delimiters,
                     const std::string& escapeCharacters, const std::string& quoteCharacters, const char eolMarker)
    : stream_(stream), hasHeaders_(firstLineContainsHeaders), eolMarker_(eolMarker), currentLine_(Null<Size>()),
      numberOfColumns_(Null<Size>()),
      tokenizer_(std::string(), boost::escaped_list_separator<char>(escapeCharacters, delimiters, quoteCharacters)) {

    // create pointer to stream and call csvreader

    // std::ifstream file_(c_str()); // Open the file using ifstream for error checking
    // std::ostringstream oss;
    // oss << file_.rdbuf(); // Read the file contents into an ostringstream

    // std::istringstream iss(oss.str()); // Create an istream from the ostringstream

    // use iss as an istream to read from the file contents

    // QL_REQUIRE(iss, "CSVFileReader: error opening file " << fileName);

    if (firstLineContainsHeaders) {
        QL_REQUIRE(!file_->eof(), "CSVReader: stream is empty");
        std::string line;
        getline(*stream_, line, eolMarker);
        boost::trim(line);
        tokenizer_.assign(line);
        std::copy(tokenizer_.begin(), tokenizer_.end(), std::back_inserter(headers_));
        numberOfColumns_ = headers_.size();
    }
}

/*
CSVFileReader::CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                             const std::string& delimiters, const std::string& escapeCharacters,
                             const std::string& quoteCharacters, const char eolMarker)
    : fileName_(fileName), hasHeaders_(firstLineContainsHeaders), eolMarker_(eolMarker), currentLine_(Null<Size>()),
      numberOfColumns_(Null<Size>()),
      tokenizer_(std::string(), boost::escaped_list_separator<char>(escapeCharacters, delimiters, quoteCharacters)) {
    file_.open(fileName);
    QL_REQUIRE(file_.is_open(), "CSVFileReader: error opening file " << fileName);
    if (firstLineContainsHeaders) {
        QL_REQUIRE(!file_.eof(), "CSVFileReader: file is empty: " << fileName);
        std::string line;
        getline(file_, line, eolMarker);
        boost::trim(line);
        tokenizer_.assign(line);
        std::copy(tokenizer_.begin(), tokenizer_.end(), std::back_inserter(headers_));
        numberOfColumns_ = headers_.size();
    }
}
*/
void setStream(std::string stream) { 
    //std::istream iss(stream);
    std::istringstream iss(std::move(stream));
    //std::istringstream iss(stream);
    //std::string line;
    //while (std::getline(stream, line)) {
       // std::cout << line << std::endl;
   // }
    // std::istream* stream;
}
const std::vector<std::string>& CSVReader::fields() const {
    QL_REQUIRE(hasHeaders_, "CSVFileReader: no headers were specified for \"" << fileName_ << "\"");
    return headers_;
}

const bool CSVReader::hasField(const std::string& field) const {
    return std::find(fields().begin(), fields().end(), field) != fields().end();
}

Size CSVReader::numberOfColumns() const {
    QL_REQUIRE(numberOfColumns_ != Null<Size>(), "CSVFileReader: number of columns not known (need call to next())");
    return numberOfColumns_;
}

bool CSVReader::next() {
    // QL_REQUIRE(file_->is_open(), "CSVFileReader: file is not open, can not move to next line");
    std::string line = "";
    // skip empty lines
    while (line.size() == 0 && !file_->eof()) {
        getline(*file_, line, eolMarker_);
        boost::trim(line);
    }
    if (file_->eof() && line.empty()) {
        close();
        return false;
    }
    if (currentLine_ == Null<Size>())
        currentLine_ = 0;
    else
        ++currentLine_;
    tokenizer_.assign(line);
    data_.clear();
    std::copy(tokenizer_.begin(), tokenizer_.end(), std::back_inserter(data_));
    if (numberOfColumns_ == Null<Size>())
        numberOfColumns_ = data_.size();
    else
        QL_REQUIRE(data_.size() == numberOfColumns_, "CSVFileReader: data line #"
                                                         << currentLine_ << " has " << data_.size()
                                                         << " fields, expected " << numberOfColumns_);
    return true;
}

Size CSVReader::currentLine() const {
    QL_REQUIRE(currentLine_ != Null<Size>(), "CSVFileReader: current line not known (need call to next())");
    return currentLine_;
}

std::string CSVReader::get(const std::string& field) const {
    QL_REQUIRE(hasHeaders_, "CSVFileReader: can not get data by field, file does not have headers");
    QL_REQUIRE(currentLine_ != Null<Size>(), "CSVFileReader: can not get data, need call to next() first");
    Size index = std::find(headers_.begin(), headers_.end(), field) - headers_.begin();
    QL_REQUIRE(index < headers_.size(), "CSVFileReader: field \"" << field << "\" not found.");
    QL_REQUIRE(index < data_.size(), "CSVFileReader: unexpected data size (" << data_.size() << "), required at least "
                                                                             << index + 1 << ", while reading field \""
                                                                             << field << "\"");
    return data_[index];
}

std::string CSVReader::get(const Size column) const {
    QL_REQUIRE(column < numberOfColumns_,
               "CSVFileReader: column " << column << " out of bounds 0..." << (numberOfColumns_ - 1));
    QL_REQUIRE(column < data_.size(),
               "CSVFileReader: unexpected data size (" << data_.size() << "), while reading column " << column);
    return data_[column];
}

// void CSVFileReader::close() { file_->close(); }
CSVFileReader::CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                             const std::string& delimiters, const std::string& escapeCharacters,
                             const std::string& quoteCharacters, const char eolMarker)
    : fileName_(fileName), CSVReader(new std::istringstream(), firstLineContainsHeaders, delimiters, escapeCharacters,
                                     quoteCharacters, eolMarker) {

    // take file name, create the stream, pass to csvreader

    std::ifstream file(fileName_.c_str()); // Open the file using ifstream for error checking
    if (!file.is_open()) {
        throw std::runtime_error("CSVFileReader: error opening file " + fileName);
    }

    // open file
    fileName_.open(fileName_);

    // pass stream to function set stream
    setStream(fileName_);

    /*

    std::ostringstream oss;
    oss << file.rdbuf(); // Read the file contents into an ostringstream

    // std::istringstream iss(oss.str()); // Create an istream from the ostringstream

    // Pass the file contents as a string to the CSVReader constructor
    csvReader_ = CSVReader(new std::istringstream(oss.str()), hasHeaders_, delimiters, escapeCharacters,
                           quoteCharacters, eolMarker_);
    */

} // namespace data
} // namespace ore
