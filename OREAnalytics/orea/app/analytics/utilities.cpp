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

#include <orea/app/analytics/utilities.hpp>
#include <orea/scenario/scenariofilereader.hpp>
#include <orea/scenario/simplescenariofactory.hpp>

using namespace ore::data;
using namespace QuantExt;

namespace ore {
namespace analytics {

std::set<std::string> parseListOfValuesToSet(const string& s) {
    std::vector<string> v = parseListOfValues(s);
    return std::set<std::string>(v.begin(), v.end());
}

QuantLib::ext::shared_ptr<ScenarioReader> loadScenarioReader(const std::string& s, const std::filesystem::path& inputPath) {
    std::filesystem::path baseScenarioPath(inputPath / s);
    if (exists(baseScenarioPath) && is_regular_file(baseScenarioPath)) {
        return ext::make_shared<ScenarioFileReader>(baseScenarioPath.string(), ext::make_shared<SimpleScenarioFactory>(false));
    } else {
        // If the file does not exist or fails, assume it is a scenario string
        return ext::make_shared<ScenarioBufferReader>(s, ext::make_shared<SimpleScenarioFactory>(true));
    }
}

std::map<std::string, TimeSeries<Real>> loadDeterministicInitialMarginFromFile(const std::string& fileName) {
    std::map<std::string, TimeSeries<Real>> result;
    // Minimum requirement: The file has a header line and contains at least
    // columns "Date", "NettingSet" and "InitialMargin".
    // We don't assume that data is sorted by netting set or date.
    ore::data::CSVFileReader reader(fileName, true);
    std::map<std::string, std::map<Date, Real>> data;
    while (reader.next()) {
        Date date = parseDate(reader.get("Date"));
        std::string nettingSet = reader.get("NettingSet");
        Real initialMargin = parseReal(reader.get("InitialMargin"));
        // LOG("IM Evolution NettingSet " << nettingSet << ": "
        //     << io::iso_date(date) << " " << initialMargin);
        if (data.find(nettingSet) == data.end())
            data[nettingSet] = std::map<Date, Real>();
        std::map<Date, Real>& evolution = data[nettingSet];
        evolution[date] = initialMargin;
    }
    for (auto d : data) {
        std::string n = d.first;
        LOG("Loading IM evolution for netting set " << n << ", size " << d.second.size());
        vector<Real> im;
        vector<Date> date;
        for (auto row : d.second) {
            im.push_back(row.second);
            date.push_back(row.first);
        }
        TimeSeries<Real> ts(date.begin(), date.end(), im.begin());
        // for (auto d : ts.dates())
        //     LOG("TimeSeries " << io::iso_date(d) << " " << ts[d]);
        result[n] = ts;
        WLOG("External IM evolution for NettingSet " << n << " loaded");
    }
    return result;
}

std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> loadCorrelationDataFromFile(const std::string& fileName) {
    ore::data::CSVFileReader reader(fileName, true);
    return loadCorrelationData(reader);
}

std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> loadCorrelationData(ore::data::CSVReader& reader) {
    std::map<std::pair<RiskFactorKey, RiskFactorKey>, Real> data;
    std::vector<std::string> dummy;
    while (reader.next()) {
        data[std::make_pair(*parseRiskFactorKey(reader.get(0), dummy),
                                        *parseRiskFactorKey(reader.get(1), dummy))] =
            ore::data::parseReal(reader.get(2));
    }
    LOG("Read " << data.size() << " valid covariance data lines");
    return data;
}

std::vector<QuantLib::Real> parseListOfRealValues(const std::string& s) {
    return parseListOfValues<QuantLib::Real>(s, &parseReal);
};

std::vector<QuantLib::Size> parseListOfIntegerValues(const std::string& s) {
    return parseListOfValues<QuantLib::Size>(s, &parseInteger);
};

std::vector<QuantLib::Period> parseListOfPeriodValues(const std::string& s) {
    return parseListOfValues<QuantLib::Period>(s, &parsePeriod);
};

std::vector<std::string> parseListOfStringValues(const std::string& s) { 
    return parseListOfValues(s); 
};

std::vector<RiskFactorKey::KeyType> parseListOfRiskFactorKeyValues(const std::string& s) {
    return parseListOfValues<RiskFactorKey::KeyType>(s, &parseRiskFactorKeyType);
};

} // namespace analytics
} // namespace ore