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

/*! \file orea/scenario/historicalscenarioloader.hpp
    \brief historical scenario loader
    \ingroup scenario
*/

#pragma once

#include <boost/make_shared.hpp>
#include <orea/scenario/scenario.hpp>
#include <orea/scenario/scenariofactory.hpp>
#include <orea/scenario/scenarioreader.hpp>
#include <ql/time/calendar.hpp>
#include <vector>

namespace ore {
namespace analytics {

class ScenarioLoader {
public:
    ScenarioLoader(){};
    
    //! Number of scenarios
    QuantLib::Size numScenarios() const { return scenarios_.size() * scenarios_.front().size(); }

    const std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& getScenarios(Size i) {
        QL_REQUIRE(i < scenarios_.size(), "Invalid index " << i << " for scenarios");
        return scenarios_.at(i);
    }

    //! Set scenarios
    std::vector<std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ore::analytics::Scenario>>>& scenarios() {
        return scenarios_;
    }

    //! The scenarios
    const std::vector<std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ore::analytics::Scenario>>>&
    scenarios() const {
        return scenarios_;
    }

    // add a scenario
    void add(const QuantLib::Date& date, Size index, const QuantLib::ext::shared_ptr<ore::analytics::Scenario>& scenario);

protected:
    // to be populated by derived classes
    std::vector<std::map<QuantLib::Date, QuantLib::ext::shared_ptr<ore::analytics::Scenario>>> scenarios_;
};

//! Class for loading historical scenarios
class SimpleScenarioLoader : public ScenarioLoader {
public:
    //! Default constructor
    SimpleScenarioLoader() {}

    /*! Constructor that loads scenarios, read from \p scenarioReader
        \warning The scenarios coming from \p scenarioReader must be in ascending order. If not,
                 an exception is thrown.
    */
    SimpleScenarioLoader(
        //! A scenario reader that feeds the loader with scenarios
        const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader);

    QuantLib::Size samples() { return indexMap_.size(); }

protected:
    std::map<std::string, QuantLib::Size> indexMap_;
};

//! Class for loading historical scenarios
class HistoricalScenarioLoader : public ScenarioLoader {
public:
    //! Default constructor
    HistoricalScenarioLoader() {}

    /*! Constructor that loads scenarios, read from \p scenarioReader, between \p startDate
        and \p endDate.

        \warning The scenarios coming from \p scenarioReader must be in ascending order. If not,
                 an exception is thrown.
    */
    HistoricalScenarioLoader(
        //! A scenario reader that feeds the loader with scenarios
        const QuantLib::ext::shared_ptr<ScenarioReader>& scenarioReader,
        //! The first date to load a scenario for
        const QuantLib::Date& startDate,
        //! The last date to load a scenario for
        const QuantLib::Date& endDate,
        //! Calendar to use when advancing dates
        const QuantLib::Calendar& calendar);

     /*! Constructor that loads scenarios, read from \p scenarioReader, for given dates */
    HistoricalScenarioLoader(
        //! A scenario reader that feeds the loader with scenarios
        const boost::shared_ptr<ScenarioReader>& scenarioReader,
        //! The first date to load a scenario for
        const std::set<QuantLib::Date>& dates);

     /*! Constructor that loads scenarios from a vector */
    HistoricalScenarioLoader(
        //! A vector of scenarios
        const std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& scenarios,
        //! The first date to load a a scenario for
        const std::set<QuantLib::Date>& dates);

    //! Get a Scenario for a given date
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> getScenario(const QuantLib::Date& date) const;

    std::vector<QuantLib::Date> dates() { 
        std::vector<QuantLib::Date> dts;
        for (const auto& s : scenarios_[0])
            dts.push_back(s.first);
        return dts;
    }
};

} // namespace analytics
} // namespace ore
