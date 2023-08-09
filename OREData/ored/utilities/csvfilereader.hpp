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

/*! \file ored/utilities/csvfilereader.hpp
    \brief utility class to access CSV files
    \ingroup utilities
*/

#pragma once

#include <ql/types.hpp>

#include <boost/tokenizer.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

namespace ore {
namespace data {
using QuantLib::Size;

class CSVReader {
public:
    /*! Ctor */
    CSVReader( const bool firstLineContainsHeaders, const std::string& delimiters = ",;\t",
              const std::string& escapeCharacters = "\\", const std::string& quoteCharacters = "\"",
              const char eolMarker = '\n');

    virtual ~CSVReader() {} // Declare virtual destructor

    /*! Set stream for function */
    void setStream(std::istream* stream);
    //void setStream(std::string);
    /*! Returns the fields, if a header line is present, otherwise throws */
    const std::vector<std::string>& fields() const;
    /*! Return true if a field is present */
    const bool hasField(const std::string& field) const;
    /*! Returns the number of columns */
    Size numberOfColumns() const;
    /*! Go to next line in file, returns false if there are no more lines */
    bool next();
    /*! Number of the current data line */
    Size currentLine() const;
    /*! Get content of field in current data line, throws if field is not present */
    std::string get(const std::string& field) const;
    /*! Get content of column in current data line, throws if column is out of range */
    std::string get(const Size column) const;
    /*! Close the file */
    virtual void close() {}

private:
    std::istream* stream_;
    const bool hasHeaders_;
    const char eolMarker_;
    Size currentLine_, numberOfColumns_;
    boost::tokenizer<boost::escaped_list_separator<char>> tokenizer_;
    std::vector<std::string> headers_, data_;
};

class CSVFileReader : public CSVReader {
public:
    /*! Ctor */
    CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                  const std::string& delimiters = ",;\t", const std::string& escapeCharacters = "\\",
                  const std::string& quoteCharacters = "\"", const char eolMarker = '\n');
    /*! Close the file */
    void close() override;

private:
    const std::string fileName_;
    std::ifstream* file_;
};

class CSVBufferReader : public CSVReader {
public:
    /*! Ctor */
    CSVBufferReader(const std::string& CSVBuffer, const bool firstLineContainsHeaders,
                  const std::string& delimiters = ",;\t", const std::string& escapeCharacters = "\\",
                  const std::string& quoteCharacters = "\"", const char eolMarker = '\n');

private:
    const std::string bufferName_;
    
};

} // namespace data
} // namespace ore



