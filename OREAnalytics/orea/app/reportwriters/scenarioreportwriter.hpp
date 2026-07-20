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

/*! \file orea/app/reportwriters/scenarioreportwriter.hpp
  \brief A Class to write ORE Scenario outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/app/reportwriter.hpp>
#include <orea/scenario/scenario.hpp>

namespace ore {
namespace data {
class Report;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class ScenarioGenerator;
class HistoricalScenarioGenerator;
class HistoricalScenarioLoader;
class ScenarioSimMarket;
class ScenarioSimMarketParameters;

//! Write ORE Scenario outputs to reports
/*! \ingroup app
 */
class ScenarioReportWriter : public ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    ScenarioReportWriter(const std::string& nullString = "#NA") : ReportWriter(nullString) {}

    virtual void writeScenarioStatistics(const QuantLib::ext::shared_ptr<ScenarioGenerator>& generator,
                                         const std::vector<RiskFactorKey>& keys, QuantLib::Size numPaths,
                                         const std::vector<QuantLib::Date>& dates, ore::data::Report& report);

    virtual void writeScenarioDistributions(const QuantLib::ext::shared_ptr<ScenarioGenerator>& generator,
                                            const std::vector<RiskFactorKey>& keys, QuantLib::Size numPaths,
                                            const std::vector<QuantLib::Date>& dates, QuantLib::Size distSteps,
                                            ore::data::Report& report);

    virtual void writeHistoricalScenarioDetails(const QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& generator,
                                                ore::data::Report& report);

                                                void writeHistoricalScenarios(const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& hsloader,
                                  const QuantLib::ext::shared_ptr<ore::data::Report>& report);

    void
    writeHistoricalScenarioDistributions(QuantLib::ext::shared_ptr<HistoricalScenarioGenerator>& hsgen,
                                         const QuantLib::ext::shared_ptr<ScenarioSimMarket>& simMarket,
                                         const QuantLib::ext::shared_ptr<ScenarioSimMarketParameters>& simMarketParams,
                                         QuantLib::ext::shared_ptr<ore::data::Report> histScenDetailsReport,
                                         QuantLib::ext::shared_ptr<ore::data::Report> statReport,
                                         QuantLib::ext::shared_ptr<ore::data::Report> distReport,
                                         QuantLib::Size distSteps = QuantLib::Null<QuantLib::Size>());
};

} // namespace analytics
} // namespace ore
