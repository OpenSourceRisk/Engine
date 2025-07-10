/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#include <orea/engine/sensitivityreportstream.hpp>
#include <orea/scenario/shiftscenariogenerator.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

#include <boost/algorithm/string.hpp>

using ore::data::parseBool;
using ore::data::parseReal;
using std::getline;
using std::string;
using std::vector;

namespace ore {
namespace analytics {

SensitivityRecord SensitivityReportStream::next() {
    row_++;
    if (row_ <= report_->rows()) {
        vector<Report::ReportType> entries;
        for (Size i = 0; i < report_->columns(); i++) {
            entries.push_back(report_->data(i, row_ - 1));
        }
        return processRecord(entries);
    }

    // If we get to here, no more lines to process so return empty record
    return SensitivityRecord();
}

void SensitivityReportStream::reset() {
    row_ = 0;
}

SensitivityRecord SensitivityReportStream::processRecord(const vector<Report::ReportType>& entries) const {

    QL_REQUIRE(entries.size() == 10, "On row number " << row_ << ": A sensitivity record needs 10 entries");

    SensitivityRecord sr;
    sr.tradeId = boost::get<std::string>(entries[0]);
    sr.isPar = parseBool(boost::get<std::string>(entries[1]));

    auto p = QuantExt::deconstructFactor(boost::get<std::string>(entries[2]));
    sr.key_1 = p.first;
    sr.desc_1 = p.second;
    sr.shift_1 = boost::get<Real>(entries[3]);

    p = QuantExt::deconstructFactor(boost::get<std::string>(entries[4]));
    sr.key_2 = p.first;
    sr.desc_2 = p.second;
    sr.shift_2 = boost::get<Real>(entries[5]);

    sr.currency = boost::get<std::string>(entries[6]);
    sr.baseNpv = boost::get<Real>(entries[7]);
    Real delta = boost::get<Real>(entries[8]);
    sr.delta = delta != Null<Real>() ? delta : 0;
    Real gamma = boost::get<Real>(entries[9]);
    sr.gamma = gamma != Null<Real>() ? gamma : 0;

    return sr;
}

} // namespace analytics
} // namespace ore
