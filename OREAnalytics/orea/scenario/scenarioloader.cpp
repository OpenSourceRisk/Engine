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

#include <orea/scenario/scenarioloader.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using QuantLib::io::iso_date;
using namespace QuantLib;

namespace ore {
namespace analytics {

void ScenarioLoader::add(const QuantLib::Date& date, Size index,
    const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario) {
    if (index >= scenarios_.size()) {
        scenarios_.push_back({});
    }

    auto& scenMap = scenarios_[index];
    auto it = scenMap.find(date);
    QL_REQUIRE(it == scenMap.end(), "Scenario already loaded for date, index pair");
    scenMap[date] = scenario;
}

SimpleScenarioLoader::SimpleScenarioLoader(const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader) {
    while (scenarioReader->next()) {
        auto scenario = scenarioReader->scenario();
        Date scenarioDate = scenario->asof();
        string label = scenario->label();
        Size index;
        auto it = indexMap_.find(label);
        if (it == indexMap_.end()) {
            index = scenarios_.size();
            indexMap_[label] = index;
        } else
            index = it->second;

        add(scenarioDate, index, scenarioReader->scenario());
    }

    // We require all the same dates for each sample
    if (scenarios_.size() > 0) {
        Size ds = scenarios_.front().size();
        for (Size i = 1; i < scenarios_.size(); i++) {
            QL_REQUIRE(ds = scenarios_[i].size(), "Number of dates must be the same for each scenario label.");   
        }
    }
}

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioLoader::getScenario(const QuantLib::Date& date) const {
    QL_REQUIRE(scenarios_.size() == 1, "No historical scenarios Loaded");
    auto it = scenarios_[0].find(date);
    QL_REQUIRE(it != scenarios_[0].end(), "ScenarioLoader can't find scenarios for date " << date);
    return it->second;
};

HistoricalScenarioLoader::HistoricalScenarioLoader(const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader,
                                                   const Date& startDate, const Date& endDate,
                                                   const Calendar& calendar) {

    QL_REQUIRE(scenarioReader, "The historical scenario loader must be provided with a valid scenario reader");

    LOG("Loading historical scenarios from " << startDate << " to " << endDate);

    // Variable used to ensure that scenarios from scenario reader are ordered ascending
    Date previousDate;

    // d will hold the dates on which we request historical scenarios in the loop below
    Date d = calendar.adjust(startDate);

    while (scenarioReader->next() && d <= endDate) {
        Date scenarioDate = scenarioReader->date();
        QL_REQUIRE(previousDate < scenarioDate, "Require that the scenario reader provides dates in "
                                                    << "ascending order but we got: " << iso_date(previousDate)
                                                    << " >= " << iso_date(scenarioDate));
        previousDate = scenarioDate;

        // If request date (d) is less than the scenario date, advance the request
        // date until it is greater than or equal to the first scenario date but
        // still less than or equal to the end date
        if (scenarioDate > d) {
            while (d < scenarioDate && d <= endDate) {
                DLOG("No data in file for date " << iso_date(d));
                d = calendar.advance(d, 1 * Days);
            }
        }

        // Skip loading a scenario if its date is before the request date
        if (scenarioDate < d) {
            DLOG("Skipping scenario for date " << iso_date(scenarioDate) << " as it is before next requested date "
                                               << iso_date(d));
            continue;
        }

        // If we get to here request date (d) must be equal to the scenario's date
        if (d <= endDate) {
            // create scenario and store it
            DLOG("Loading scenario for date " << iso_date(d));
            add(d, 0, scenarioReader->scenario());

            // Advance the request date
            d = calendar.advance(d, 1 * Days);
        } else {
            DLOG("Skipping scenario for date " << iso_date(scenarioDate) << " as it is past the loader's end date "
                                               << iso_date(endDate));
        }
    }

    LOG("Loaded " << scenarios_.size() << " from " << startDate << " to " << endDate);
}

HistoricalScenarioLoader::HistoricalScenarioLoader(
    const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader,
    const std::set<Date>& dates) {
    while (scenarioReader->next()) {
        Date scenarioDate = scenarioReader->date();

        auto it = dates.find(scenarioDate);
        if (it == dates.end())
            continue;
        else {
            add(scenarioDate, 0, scenarioReader->scenario());   
        }
        if (scenarios_.size() == dates.size())
            break;
    }
}

HistoricalScenarioLoader::HistoricalScenarioLoader(
    const std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& scenarios,
    const std::set<QuantLib::Date>& dates) {
    for (const auto& s : scenarios) {
        Date scenarioDate = s->asof();

        auto it = dates.find(scenarioDate);
        if (it == dates.end())
            continue;
        else
            add(scenarioDate, 0, s);
        if (scenarios_.size() == dates.size())
            break;
    }
}

} // namespace analytics
} // namespace ore
