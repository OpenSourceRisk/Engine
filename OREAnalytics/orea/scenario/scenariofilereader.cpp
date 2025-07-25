/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <orea/scenario/scenariofilereader.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/errors.hpp>

using ore::data::CSVFileReader;
using ore::data::parseDate;
using ore::data::parseReal;
using std::string;
using namespace QuantLib;

namespace ore {
namespace analytics {

ScenarioCSVReader::ScenarioCSVReader(const QuantLib::ext::shared_ptr<ore::data::CSVReader>& reader,
                                       const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory)
    : scenarioFactory_(scenarioFactory), reader_(reader), finished_(false) {

    // Do some checks
    QL_REQUIRE(reader_->fields().size() >= 4, "Need at least 4 columns" );
    QL_REQUIRE(reader_->fields()[0] == "Date", "First column must be 'Date'");
    QL_REQUIRE(reader_->fields()[1] == "Scenario", "Second column should be 'Scenario'");
    QL_REQUIRE(reader_->fields()[2] == "Numeraire", "Third column should be 'Numeraire'");

    // Populate the risk factor keys vector
    keys_.reserve(reader_->fields().size() - 3);
    for (Size k = 3; k < reader_->fields().size(); ++k) {
        keys_.push_back(QuantExt::parseRiskFactorKey(reader_->fields()[k]));
    }
}

bool ScenarioCSVReader::next() {
    finished_ = reader_->next() ? false : true;
    return !finished_;
}

Date ScenarioCSVReader::date() const {
    if (finished_) {
        return Null<Date>();
    } else {
        return parseDate(reader_->get("Date"));
    }
}

QuantLib::ext::shared_ptr<ore::analytics::Scenario> ScenarioCSVReader::scenario() const {
    if (finished_) {
        return nullptr;
    } else {
        Date date = parseDate(reader_->get("Date"));
        Real numeraire = parseReal(reader_->get("Numeraire"));
        string label = reader_->get("Scenario");
        TLOG("Creating scenario for date " << io::iso_date(date));
        QuantLib::ext::shared_ptr<Scenario> scenario =
            scenarioFactory_->buildScenario(date, true, false, label, numeraire);
        Real value;
        for (Size k = 0; k < keys_.size(); ++k) {
            if (ore::data::tryParseReal(reader_->get(k + 3), value))
                scenario->add(keys_[k], value);
        }
        return scenario;
    }
}

ScenarioFileReader::~ScenarioFileReader() {
    // Close the file
    auto file = QuantLib::ext::dynamic_pointer_cast<CSVFileReader>(reader_);
    QL_REQUIRE(file, "ScenarioCSVReader: reader is not a CSVFileReader");
    file->close();
    LOG("The file has been closed");
}

} // namespace analytics
} // namespace ore
