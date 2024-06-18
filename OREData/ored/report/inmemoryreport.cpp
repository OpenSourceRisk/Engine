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

#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/serializationdate.hpp>
#include <ored/utilities/serializationperiod.hpp>

#include <boost/algorithm/string/join.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/variant.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#include <fstream>

namespace ore {
namespace data {

Report& InMemoryReport::addColumn(const string& name, const ReportType& rt, Size precision) {
    headers_.push_back(name);
    columnTypes_.push_back(rt);
    columnPrecision_.push_back(precision);
    data_.push_back(vector<ReportType>()); // Initialise vector for column
    i_++;
    return *this;
}

Report& InMemoryReport::next() {
    QL_REQUIRE(i_ == headers_.size(), "Cannot go to next line, only " << i_ << " entries filled, report headers are: "
                                                                      << boost::join(headers_, ","));
    i_ = 0;
    if (bufferSize_ && data_[0].size() == bufferSize_ && !headers_.empty()) {
        std::string s = std::tmpnam(nullptr);
        std::ofstream os(s.c_str(), std::ios::binary);
        boost::archive::binary_oarchive oa(os, boost::archive::no_header);
        for (Size i = 0; i < headers_.size(); i++) {
            oa << data_[i];
            data_[i].clear();
        }
        os.close();
        files_.push_back(s);
    }
    return *this;
}

Report& InMemoryReport::add(const ReportType& rt) {
    // check type is valid
    QL_REQUIRE(i_ < headers_.size(), "No column to add [" << rt << "] to.");
    QL_REQUIRE(rt.which() == columnTypes_[i_].which(), "Cannot add value "
                                                           << rt << " of type " << rt.which() << " to column "
                                                           << headers_[i_] << " of type " << columnTypes_[i_].which()
                                                           << ", report headers are: " << boost::join(headers_, ","));

    data_[i_].push_back(rt);
    i_++;
    return *this;
}

Report& InMemoryReport::add(const InMemoryReport& report) {
    QL_REQUIRE(columns() == report.columns(), "Cannot combine reports of different sizes ("
                                                  << columns() << " vs " << report.columns()
                                                  << "), report headers are: " << boost::join(headers_, ","));
    end();
    for (Size i = 0; i < columns(); i++) {
        string h1 = headers_[i];
        string h2 = report.header(i);
        QL_REQUIRE(h1 == h2, "Cannot combine reports with different headers (\""
                                 << h1 << "\" and \"" << h2
                                 << "\"), report headers are: " << boost::join(headers_, ","));
    }

    if (i_ == headers_.size())
        next();
        
    for (Size rowIdx = 0; rowIdx < report.rows(); rowIdx++) {
        for (Size columnIdx = 0; columnIdx < report.columns(); columnIdx++) {
            add(report.data(columnIdx)[rowIdx]);
        }
        next();
    }
        
    return *this;
}

void InMemoryReport::end() {
    QL_REQUIRE(i_ == headers_.size() || i_ == 0, "report is finalized with incomplete row, got data for "
                                                     << i_ << " columns out of " << columns()
                                                     << ", report headers are: " << boost::join(headers_, ","));
}

const vector<Report::ReportType>& InMemoryReport::data(Size i) const {
    QL_REQUIRE(files_.empty(), "Member function InMemoryReport::data() is not supported "
        "when buffering is active");
    QL_REQUIRE(data_[i].size() == rows(), "internal error: report column "
                                              << i << " (" << header(i) << ") contains " << data_[i].size()
                                              << " rows, expected are " << rows()
                                              << " rows, report headers are: " << boost::join(headers_, ","));
    return data_[i];
}

void InMemoryReport::toFile(const string& filename, const char sep, const bool commentCharacter, char quoteChar,
                            const string& nullString, bool lowerHeader) {

    CSVFileReport cReport(filename, sep, commentCharacter, quoteChar, nullString, lowerHeader);

    for (Size i = 0; i < headers_.size(); i++) {
        cReport.addColumn(headers_[i], columnTypes_[i], columnPrecision_[i]);
    }

    auto numColumns = columns();
    if (numColumns > 0) {

        for (auto &f : files_) {
            vector<vector<ReportType>> data(numColumns);
            std::ifstream is(f.c_str(), std::ios::binary);
            boost::archive::binary_iarchive ia(is, boost::archive::no_header);
            for (Size i = 0; i < numColumns; i++) {
                ia >> data[i];
            }
            is.close();

            for (Size i = 0; i < data[0].size(); i++) {
                cReport.next();
                for (Size j = 0; j < numColumns; j++) {
                    cReport.add(data[j][i]);
                }
            }
        }

        auto numRows = data_[0].size();

        for (Size i = 0; i < numRows; i++) {
            cReport.next();
            for (Size j = 0; j < numColumns; j++) {
                cReport.add(data_[j][i]);
            }
        }
    }

    cReport.end();
}

} // namespace data
} // namespace ore
