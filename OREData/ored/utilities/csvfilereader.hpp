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
    \brief utility class to acess CSV files
    \ingroup utilities
*/

#pragma once

#include <ql/types.hpp>

#include <fstream>
#include <vector>

using QuantLib::Size;

namespace ore {
namespace data {

class CSVFileReader {
public:
    /*! Ctor */
    CSVFileReader(const std::string& fileName, const bool firstLineContainsHeaders,
                  const std::string& delimiters = ",;\t", const char eolMarker = '\n');

    /*! Returns the fields, if a hedaer line is present, otherwise throws */
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
    void close();

private:
    const std::string fileName_;
    const bool hasHeaders_;
    const std::string delimiters_;
    const char eolMarker_;
    std::ifstream file_;
    Size currentLine_, numberOfColumns_;
    std::vector<std::string> headers_, data_;
};

} // namespace data
} // namespace ore
