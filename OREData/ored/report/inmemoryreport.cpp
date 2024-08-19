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
#include <boost/filesystem.hpp>

#include <fstream>

namespace ore {
namespace data {

InMemoryReport::~InMemoryReport() {
    for (const auto &f : files_)
        std::remove(f.c_str());
}

Report& InMemoryReport::addColumn(const string& name, const ReportType& rt, Size precision) {
    headers_.push_back(name);
    columnTypes_.push_back(rt);
    columnPrecision_.push_back(precision);
    data_.push_back(vector<ReportType>()); // Initialise vector for column
    headersMap_[name] = i_;
    i_++;
    return *this;
}

Report& InMemoryReport::next() {
    QL_REQUIRE(i_ == headers_.size(), "Cannot go to next line, only " << i_ << " entries filled, report headers are: "
                                                                      << boost::join(headers_, ","));
    i_ = 0;
    if (bufferSize_ && data_[0].size() == bufferSize_ && !headers_.empty()) {
        // The size of data_ has hit the buffer limit - flush the contents of data_ to disk
        if (files_.empty()) {
            // On the first pass through this loop, initialize the cache, then below we copy data_ -> cache_
            cache_.clear();
            cacheIndex_ = 0;
        }
        boost::filesystem::path p = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
        std::string s = p.string();
        std::ofstream os(s.c_str(), std::ios::binary);
        boost::archive::binary_oarchive oa(os, boost::archive::no_header);
        for (Size i = 0; i < headers_.size(); i++) {
            oa << data_[i];
            if (files_.empty())
                cache_.push_back(data_[i]); // The first time we flush data_ to disk, we also copy data_ -> cache_
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
            add(report.data(columnIdx, rowIdx));
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

const vector<vector<Report::ReportType>>& InMemoryReport::cache(Size cacheIndex) const {
    if (cache_.empty() || cacheIndex_ != cacheIndex) {
        auto numColumns = columns();
        cache_.clear();
        cache_.resize(numColumns);
        std::ifstream is(files_[cacheIndex].c_str(), std::ios::binary);
        boost::archive::binary_iarchive ia(is, boost::archive::no_header);
        for (Size i = 0; i < numColumns; i++)
            ia >> cache_[i];
        is.close();
        cacheIndex_ = cacheIndex;
    }
    return cache_;
}

const Report::ReportType& InMemoryReport::dataImpl(const vector<vector<ReportType>>& data, Size i, Size j, Size expectedSize) const {
    QL_REQUIRE(data[i].size() == expectedSize, "internal error: report column "
        << i << " (" << header(i) << ") contains " << data[i].size()
        << " rows, expected are " << expectedSize
        << " rows, report headers are: " << boost::join(headers_, ","));
    return data[i][j];
}

const Report::ReportType& InMemoryReport::data(Size i, Size j) const {
    if (files_.empty()) {
        // Buffering is not active - retrieve the requested data from the data_ container
        return dataImpl(data_, i, j, rows());
    } else {
        // Buffering is active
        Size bufferRowCount = files_.size() * bufferSize_;
        if (j < bufferRowCount) {
            // The requested data is in the buffer - retrieve it and return it
            auto dv = std::div(int(j), int(bufferSize_));
            const vector<vector<ReportType>>& data = cache(dv.quot);
            return dataImpl(data, i, dv.rem, bufferSize_);
        } else {
            // The requested data is not in the buffer, it's in the data_ container so use that
            return dataImpl(data_, i, j - bufferRowCount, data_[0].size());
        }
    }
}

void InMemoryReport::toFile(const string& filename, const char sep, const bool commentCharacter, char quoteChar,
                            const string& nullString, bool lowerHeader) {

    CSVFileReport cReport(filename, sep, commentCharacter, quoteChar, nullString, lowerHeader);

    for (Size i = 0; i < headers_.size(); i++) {
        cReport.addColumn(headers_[i], columnTypes_[i], columnPrecision_[i]);
    }

    auto numColumns = columns();
    if (numColumns > 0) {

        for (Size cacheIndex = 0; cacheIndex < files_.size(); cacheIndex++) {
            const vector<vector<ReportType>>& data = cache(cacheIndex);
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
