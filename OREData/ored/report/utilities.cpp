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


#include <ored/report/utilities.hpp>

namespace ore {
namespace data {

//! Adds a column to an existing InMemoryReport, the column value will be set to *value* for all existing rows
//! Caution: This copies all existing values of the report and create a new one
// TODO: Improve the report class to allow to add new columns to avoid copies
QuantLib::ext::shared_ptr<ore::data::InMemoryReport>
addColumnToExisitingReport(const std::string& columnName, const std::string& value,
                           const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report) {
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> newReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    if (report != nullptr) {
        newReport->addColumn(columnName, string());
        for (size_t i = 0; i < report->columns(); i++) {
            newReport->addColumn(report->header(i), report->columnType(i), report->columnPrecision(i));
        }
        for (size_t row = 0; row < report->rows(); row++) {
            newReport->next();
            newReport->add(value);
            for (size_t col = 0; col < report->columns(); col++) {
                newReport->add(report->data(col)[row]);
            }
        }
        newReport->end();
    }
    return newReport;
}

QuantLib::ext::shared_ptr<ore::data::InMemoryReport>
addColumnsToExisitingReport(const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& newColsReport,
                            const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& report) {
    QuantLib::ext::shared_ptr<ore::data::InMemoryReport> newReport =
        QuantLib::ext::make_shared<ore::data::InMemoryReport>();
    if (report != nullptr && newColsReport->rows() == 1) {

        for (size_t i = 0; i < newColsReport->columns(); ++i) {
            newReport->addColumn(newColsReport->header(i), newColsReport->columnType(i),
                                 newColsReport->columnPrecision(i));
        }
        for (size_t i = 0; i < report->columns(); i++) {
            newReport->addColumn(report->header(i), report->columnType(i), report->columnPrecision(i));
        }
        for (size_t row = 0; row < report->rows(); row++) {
            newReport->next();
            for (size_t i = 0; i < newColsReport->columns(); ++i) {
                newReport->add(newColsReport->data(i)[0]);
            }
            for (size_t col = 0; col < report->columns(); col++) {
                newReport->add(report->data(col)[row]);
            }
        }
        newReport->end();
    }
    return newReport;
}

QuantLib::ext::shared_ptr<ore::data::InMemoryReport>
concatenateReports(const std::vector<QuantLib::ext::shared_ptr<ore::data::InMemoryReport>>& reports) {
    if (!reports.empty() && reports.front() != nullptr) {
        auto firstReport = reports.front();
        QuantLib::ext::shared_ptr<InMemoryReport> concatenatedReport =
            QuantLib::ext::make_shared<InMemoryReport>(*firstReport);
        for (size_t i = 1; i < reports.size(); i++) {
            if (reports[i] != nullptr) {
                concatenatedReport->add(*reports[i]);
            }
        }
        return concatenatedReport;
    }
    return nullptr;
}
} // namespace data
} // namespace ore
