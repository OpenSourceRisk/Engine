/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#include <ored/utilities/parsers.hpp>

namespace ore {
namespace analytics {


std::set<std::string> parseListOfValuesToSet(const std::string& s);

QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::string& s, const std::filesystem::path& inputPath = std::filesystem::path());

std::vector<QuantLib::Real> parseListOfRealValues(const std::string& s);
std::vector<QuantLib::Size> parseListOfIntegerValues(const std::string& s);
std::vector<QuantLib::Period> parseListOfPeriodValues(const std::string& s);
std::vector<std::string> parseListOfStringValues(const std::string& s);

std::map<std::string, TimeSeries<Real>>  loadDeterministicInitialMarginFromFile(const std::string& fileName);

std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> loadCorrelationDataFromFile(const std::string& fileName);
std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> loadCorrelationData(ore::data::CSVReader& reader);

} // namesapce analytics
} // namespace ore