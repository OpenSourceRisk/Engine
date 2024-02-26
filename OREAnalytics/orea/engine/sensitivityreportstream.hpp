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

/*! \file orea/engine/sensitivityreportstream.hpp
    \brief Class for streaming SensitivityRecords from a report
 */

#pragma once

#include <ored/report/inmemoryreport.hpp>
#include <orea/engine/sensitivitystream.hpp>

#include <fstream>
#include <string>

namespace ore {
namespace analytics {

//! Class for streaming SensitivityRecords from csv file
class SensitivityReportStream : public SensitivityStream {
public:
    //! Constructor
    SensitivityReportStream(const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report) : report_(report) {}
    virtual ~SensitivityReportStream() {} // Declare virtual destructor
        
    //! Returns the next SensitivityRecord in the stream
    SensitivityRecord next() override;
    //! Resets the stream so that SensitivityRecord objects can be streamed again
    void reset() override;

private:
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> report_;

    QuantLib::Size row_ = 0;

    //! Create a record from a collection of strings
    SensitivityRecord processRecord(const std::vector<ore::data::Report::ReportType>& entries) const;
};

} // namespace analytics
} // namespace ore
