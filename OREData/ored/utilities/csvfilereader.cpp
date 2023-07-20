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

CSVReader::CSVReader( const bool firstLineContainsHeaders, const std::string& delimiters,
                     const std::string& escapeCharacters, const std::string& quoteCharacters, const char eolMarker)
    : hasHeaders_(firstLineContainsHeaders), eolMarker_(eolMarker), currentLine_(Null<Size>()),
      numberOfColumns_(Null<Size>()),
      tokenizer_(std::string(), boost::escaped_list_separator<char>(escapeCharacters, delimiters, quoteCharacters)) {
}

void CSVReader::setStream(std::istream* stream) { 
     // set stream 
     stream_ = stream;

     if (hasHeaders_) {
         QL_REQUIRE(!stream_->eof(), "CSVReader: stream is empty");
         std::string line;
         getline(*stream_, line, eolMarker_);
         boost::trim(line);
         tokenizer_.assign(line);
         std::copy(tokenizer_.begin(), tokenizer_.end(), std::back_inserter(headers_));
         numberOfColumns_ = headers_.size();
     }
}
const std::vector<std::string>& CSVReader::fields() const {
    //QL_REQUIRE(hasHeaders_, "CSVFileReader: no headers were specified for \"" << fileName_ << "\"");
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
    //QL_REQUIRE(stream_->is_open(), "CSVFileReader: file is not open, can not move to next line");
    std::string line = "";
    // skip empty lines
    while (line.size() == 0 && !stream_->eof()) {
        getline(*stream_, line, eolMarker_);
        boost::trim(line);
    }
    if (stream_->eof() && line.empty()) {
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

CSVFileReader::CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                             const std::string& delimiters, const std::string& escapeCharacters,
                             const std::string& quoteCharacters, const char eolMarker)
    :  CSVReader(firstLineContainsHeaders, delimiters, escapeCharacters, quoteCharacters,
                eolMarker),
      fileName_(fileName) {

    // set file name
    file_ = new std::ifstream(fileName);

    // pass stream to function set stream
    setStream(file_);

}

void CSVFileReader::close() { file_->close(); }

CSVBufferReader::CSVBufferReader(const std::string& bufferName, const bool firstLineContainsHeaders,
                                 const std::string& delimiters, const std::string& escapeCharacters,
                                 const std::string& quoteCharacters, const char eolMarker)
    : CSVReader(firstLineContainsHeaders, delimiters, escapeCharacters, quoteCharacters, eolMarker),
      bufferName_(bufferName) {

    // process the buffer
    std::stringstream *buffer = new std::stringstream(bufferName);

    // set buffer stream
    setStream(buffer);

}

} // namespace data
} // namespace ore
