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

#include <orea/scenario/historicalscenarioloader.hpp>
#include <ored/utilities/csvfilereader.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

using namespace QuantLib;
using QuantLib::io::iso_date;
using namespace QuantLib;

namespace ore {
namespace analytics {

QuantLib::ext::shared_ptr<Scenario> HistoricalScenarioLoader::getHistoricalScenario(const QuantLib::Date& date) const {
    QL_REQUIRE(historicalScenarios_.size() > 0, "No Historical Scenarios Loaded");

    auto it = std::find(dates_.begin(), dates_.end(), date);
    QL_REQUIRE(it != dates_.end(), "HistoricalScenarioLoader can't find an index for date " << date);

    Size index = std::distance(dates_.begin(), it);
    return historicalScenarios_[index];
};

HistoricalScenarioLoader::HistoricalScenarioLoader(const QuantLib::ext::shared_ptr<HistoricalScenarioReader>& scenarioReader,
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
            historicalScenarios_.push_back(scenarioReader->scenario());
            dates_.push_back(d);

            // Advance the request date
            d = calendar.advance(d, 1 * Days);
        } else {
            DLOG("Skipping scenario for date " << iso_date(scenarioDate) << " as it is past the loader's end date "
                                               << iso_date(endDate));
        }
    }

    LOG("Loaded " << historicalScenarios_.size() << " from " << startDate << " to " << endDate);
}

HistoricalScenarioLoader::HistoricalScenarioLoader(
    const boost::shared_ptr<HistoricalScenarioReader>& scenarioReader,
    const std::set<Date>& dates) {
    while (scenarioReader->next()) {
        Date scenarioDate = scenarioReader->date();

        auto it = dates.find(scenarioDate);
        if (it == dates.end())
            continue;
        else {
            historicalScenarios_.push_back(scenarioReader->scenario());
            dates_.push_back(scenarioDate);            
        }
        if (dates_.size() == dates.size())
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
        else {
            historicalScenarios_.push_back(s);
            dates_.push_back(scenarioDate);
        }
        if (dates_.size() == dates.size())
            break;
    }
}

} // namespace analytics
} // namespace ore
