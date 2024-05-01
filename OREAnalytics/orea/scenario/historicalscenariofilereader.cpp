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

#include <orea/scenario/historicalscenariofilereader.hpp>

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

HistoricalScenarioFileReader::HistoricalScenarioFileReader(const string& fileName,
                                                           const QuantLib::ext::shared_ptr<ScenarioFactory>& scenarioFactory)
    : scenarioFactory_(scenarioFactory), file_(fileName, true), finished_(false) {

    // Do some checks
    QL_REQUIRE(file_.fields().size() >= 4, "Need at least 4 columns in the file " << fileName);
    QL_REQUIRE(file_.fields()[0] == "Date", "First column must be 'Date' in the file " << fileName);
    QL_REQUIRE(file_.fields()[1] == "Scenario", "Second column should be 'Scenario' in the file " << fileName);
    QL_REQUIRE(file_.fields()[2] == "Numeraire", "Third column should be 'Numeraire' in the file " << fileName);

    // Populate the risk factor keys vector
    keys_.reserve(file_.fields().size() - 3);
    for (Size k = 3; k < file_.fields().size(); ++k) {
        keys_.push_back(parseRiskFactorKey(file_.fields()[k]));
    }
}

HistoricalScenarioFileReader::~HistoricalScenarioFileReader() {
    // Close the file
    file_.close();
    LOG("The file has been closed");
}

bool HistoricalScenarioFileReader::next() {
    finished_ = file_.next() ? false : true;
    return !finished_;
}

Date HistoricalScenarioFileReader::date() const {
    if (finished_) {
        return Null<Date>();
    } else {
        return parseDate(file_.get("Date"));
    }
}

QuantLib::ext::shared_ptr<ore::analytics::Scenario> HistoricalScenarioFileReader::scenario() const {
    if (finished_) {
        return nullptr;
    } else {
        Date date = parseDate(file_.get("Date"));
        Real numeraire = parseReal(file_.get("Numeraire"));
        TLOG("Creating scenario for date " << io::iso_date(date));
        QuantLib::ext::shared_ptr<Scenario> scenario =
            scenarioFactory_->buildScenario(date, true, std::string(), numeraire);
        Real value;
        for (Size k = 0; k < keys_.size(); ++k) {
            if (ore::data::tryParseReal(file_.get(k + 3), value))
                scenario->add(keys_[k], value);
        }
        return scenario;
    }
}

} // namespace analytics
} // namespace ore
