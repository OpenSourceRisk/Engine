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

/*! \file ored/report/csvreport.hpp
    \brief CSV Report class
    \ingroup report
*/

#pragma once

#include <ored/report/report.hpp>
#include <stdio.h>
#include <vector>

namespace ore {
namespace data {

class ReportTypePrinter;
/*! CSV Report class

\ingroup report
*/
class CSVFileReport : public Report {
public:
    /*! Create a report with the given filename, will throw if it cannot open the file.
        \param filename         name of the csv file that is created
        \param sep              separator character for the csv file. It defaults to a comma.
        \param commentCharacter if \c true, the first row starts with the \c # character.
        \param quoteChar        character to use to quote strings. If not provided, strings are not quoted.
        \param nullString       string used to represent \c QuantLib::Null values or infinite values. If not provided,
                                this defaults to \c \#N/A.
        \param lowerHeader      if \c true, makes the first character of each header lower case.
        \param rolloverSize     in MB, if set we rollover over to a new csv when file size grows above this
    */
    CSVFileReport(const string& filename, const char sep = ',', const bool commentCharacter = true,
                  char quoteChar = '\0', const std::string& nullString = "#N/A", bool lowerHeader = false,
                  QuantLib::Size rolloverSize = QuantLib::Null<QuantLib::Size>());
    ~CSVFileReport();

    void open();
    void rollover();
    Report& addColumn(const string& name, const ReportType& rt, Size precision = 0) override;
    Report& next() override;
    Report& add(const ReportType& rt) override;
    void end() override;
    void flush() override;

private:
    void checkIsOpen(const std::string& op) const;

    std::vector<ReportType> columnTypes_;
    std::vector<ReportTypePrinter> printers_;
    std::string filename_, baseFilename_;
    char sep_;
    bool commentCharacter_;
    char quoteChar_;
    std::string nullString_;
    bool lowerHeader_;
    QuantLib::Size rolloverSize_;
    Size i_, j_ = 0;
    Size version_ = 0;
    FILE* fp_;
    bool finalized_ = false;
};
} // namespace data
} // namespace ore
