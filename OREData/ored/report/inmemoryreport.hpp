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

/*! \file ored/report/inmemoryreport.hpp
    \brief In memory report class
    \ingroup report
*/

#pragma once

#include <ored/report/report.hpp>
#include <ored/report/csvreport.hpp>
#include <ql/errors.hpp>
#include <vector>
namespace ore {
namespace data {
using std::string;
using std::vector;

/*! InMemoryReport just stores report information in local vectors and provides an interface to access
 *  the values. It could be used as a backend to a GUI
 \ingroup report
 */
class InMemoryReport : public Report {
public:
    InMemoryReport() : i_(0) {}

    Report& addColumn(const string& name, const ReportType& rt, Size precision = 0) {
        headers_.push_back(name);
        columnTypes_.push_back(rt);
        columnPrecision_.push_back(precision);
        data_.push_back(vector<ReportType>()); // Initalise vector for
        i_++;
        return *this;
    }

    Report& next() {
        QL_REQUIRE(i_ == headers_.size(), "Cannot go to next line, only " << i_ << " entires filled");
        i_ = 0;
        return *this;
    }

    Report& add(const ReportType& rt) {
        // check type is valid
        QL_REQUIRE(i_ < headers_.size(), "No column to add [" << rt << "] to.");
        QL_REQUIRE(rt.which() == columnTypes_[i_].which(),
                   "Cannot add value " << rt << " of type " << rt.which() << " to column " << headers_[i_]
                                       << " of type " << columnTypes_[i_].which());

        data_[i_].push_back(rt);
        i_++;
        return *this;
    }

    void end() {}

    // InMemoryInterface
    Size columns() const { return headers_.size(); }
    Size rows() const { return data_[0].size(); }
    const string& header(Size i) const { return headers_[i]; }
    ReportType columnType(Size i) const { return columnTypes_[i]; }
    Size columnPrecision(Size i) const { return columnPrecision_[i]; }
    //! Returns the data
    const vector<ReportType>& data(Size i) const { return data_[i]; }
    
    void toFile(const string& filename, const char sep = ',', const bool commentCharacter = true, char quoteChar = '\0',
                const string& nullString = "#N/A") {
        
        CSVFileReport cReport(filename, sep, commentCharacter, quoteChar,
                              nullString);
        
        
        for (Size i = 0; i < headers_.size(); i++) {
            cReport.addColumn(headers_[i], columnTypes_[i], columnPrecision_[i]);
        }
        
        auto numColumns = columns();
        auto numRows = data_[0].size();
    
        for (Size i = 0; i < numRows; i++) {
            cReport.next();
            for (Size j = 0; j < numColumns; j++) {
                cReport.add(data_[j][i]);
            }
        }
        
        cReport.end();
    }

private:
    Size i_;
    vector<string> headers_;
    vector<ReportType> columnTypes_;
    vector<Size> columnPrecision_;
    vector<vector<ReportType>> data_;
};
} // namespace data
} // namespace ore
