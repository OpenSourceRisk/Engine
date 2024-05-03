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
#include <orea/scenario/historicalscenarioreader.hpp>
#include <ql/time/calendar.hpp>
#include <vector>

namespace ore {
namespace analytics {

//! Class for loading historical scenarios
class HistoricalScenarioLoader {
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
        const QuantLib::ext::shared_ptr<HistoricalScenarioReader>& scenarioReader,
        //! The first date to load a a scenario for
        const QuantLib::Date& startDate,
        //! The last date to load a scenario for
        const QuantLib::Date& endDate,
        //! Calendar to use when advancing dates
        const QuantLib::Calendar& calendar);

     /*! Constructor that loads scenarios, read from \p scenarioReader, for given dates */
    HistoricalScenarioLoader(
        //! A scenario reader that feeds the loader with scenarios
        const boost::shared_ptr<HistoricalScenarioReader>& scenarioReader,
        //! The first date to load a a scenario for
        const std::set<QuantLib::Date>& dates);

     /*! Constructor that loads scenarios from a vector */
    HistoricalScenarioLoader(
        //! A vector of scenarios
        const std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& scenarios,
        //! The first date to load a a scenario for
        const std::set<QuantLib::Date>& dates);

    //! Get a Scenario for a given date
    QuantLib::ext::shared_ptr<ore::analytics::Scenario> getHistoricalScenario(const QuantLib::Date& date) const;
    //! Number of scenarios
    QuantLib::Size numScenarios() const { return historicalScenarios_.size(); }
    //! Set historical scenarios
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& historicalScenarios() { return historicalScenarios_; }
    //! The historical scenarios
    const std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>>& historicalScenarios() const {
        return historicalScenarios_;
    }
    //! Set historical scenario dates
    std::vector<QuantLib::Date>& dates() { return dates_; }
    //! The historical scenario dates
    const std::vector<QuantLib::Date>& dates() const { return dates_; }

protected:
    // to be populated by derived classes
    std::vector<QuantLib::ext::shared_ptr<ore::analytics::Scenario>> historicalScenarios_;
    std::vector<QuantLib::Date> dates_;
};

} // namespace analytics
} // namespace ore
