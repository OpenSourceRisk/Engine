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

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <string>

#include <orea/scenario/csvscenariogenerator.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

using ore::data::to_string;
using namespace ore::data;
using QuantLib::Size;

namespace ore {
namespace analytics {

CSVScenarioGenerator::CSVScenarioGenerator(const std::string& filename,
                                           const QuantLib::ext::shared_ptr<ScenarioFactory> scenarioFactory, const char sep)
    : sep_(sep), filename_(filename), scenarioFactory_(scenarioFactory) {
    file_.open(filename_.c_str());
    QL_REQUIRE(file_.is_open(), "error opening file " << filename_);
    readKeys();
}

void CSVScenarioGenerator::readKeys() {
    QL_REQUIRE(!file_.eof(), "error reading file: No header found in scenariofile" << filename_);
    string line;
    getline(file_, line);
    boost::trim(line);
    string strsep(1, sep_);
    vector<string> tokens;
    boost::split(tokens, line, boost::is_any_of(strsep), boost::token_compress_on);
    QL_REQUIRE(tokens.size() > 3, "No RiskFactorKeys found in " << filename_);
    keys_.resize(tokens.size() - 3);
    for (Size i = 3; i < tokens.size(); i++) {
        keys_[i - 3] = parseRiskFactorKey(tokens[i]);
    }
}
QuantLib::ext::shared_ptr<Scenario> CSVScenarioGenerator::next(const Date& d) {
    // Read in the next line
    QL_REQUIRE(!file_.eof(), "unexpected end of scenario file " << filename_);
    string line;
    getline(file_, line);

    // Split line into tokens
    vector<string> tokens;
    boost::trim(line);
    string strsep(1, sep_);
    boost::split(tokens, line, boost::is_any_of(strsep), boost::token_compress_on);

    // Check that dates match
    QL_REQUIRE(to_string(d) == tokens[0], "Incompatible date " << tokens[0] << " in " << filename_);

    // Build scenario
    const QuantLib::ext::shared_ptr<Scenario> scenario = scenarioFactory_->buildScenario(d, true);

    // Fill scenario with RiskFactorKeys
    QL_REQUIRE(keys_.size() == tokens.size() - 3, "Erroneus line in " << filename_);
    for (Size i = 3; i < tokens.size(); i++) {
        scenario->add(keys_[i - 3], parseReal(tokens[i]));
    }

    return scenario;
}

void CSVScenarioGenerator::reset() {
    file_.seekg(std::ios::beg);
    string dummy;
    getline(file_, dummy);
}

CSVScenarioGenerator::~CSVScenarioGenerator() { file_.close(); }
} // namespace analytics
} // namespace ore
