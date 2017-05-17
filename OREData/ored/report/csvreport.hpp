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

class CSVFileReport : public Report {
public:
    /*! Create a report with the given filename, will throw if it cannot open the file.
     *  sep is the separator which defaults to a comma
     */
    CSVFileReport(const string& filename, const char sep = ',');
    ~CSVFileReport();

    Report& addColumn(const string& name, const ReportType& rt, Size precision = 0) override;
    Report& next() override;
    Report& add(const ReportType& rt) override;
    void end() override;

private:
    std::vector<ReportType> columnTypes_;
    std::vector<ReportTypePrinter> printers_;
    string filename_;
    char sep_;
    Size i_;
    FILE* fp_;
};
}
}
