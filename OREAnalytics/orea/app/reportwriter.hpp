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

/*! \file orea/app/reportwriter.hpp
  \brief A Class to write ORE outputs to reports
  \ingroup app
 */

#pragma once

#include <orea/simm/imschedulecalculator.hpp>
#include <orea/simm/simmconfiguration.hpp>
#include <orea/scenario/scenario.hpp>
#include <ored/utilities/timer.hpp>
#include <ql/shared_ptr.hpp>
#include <ql/time/date.hpp>
#include <ql/types.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ore {
namespace data {
class Report;
class Market;
class Portfolio;
class Loader;
class MarketDatum;
class AdjustmentFactors;
class InMemoryReport;
class NettingSetDetails;
struct TradeCashflowReportData;
class Timer;
} // namespace data
} // namespace ore

namespace ore {
namespace analytics {

class SensitivityCube;
class NPVCube;
class HistoricalScenarioLoader;
class IMScheduleResults;
class Crif;
struct CrifRecord;

//! Write ORE outputs to reports
/*! \ingroup app
 */
class ReportWriter {
public:
    /*! Constructor.
        \param nullString used to represent string values that are not applicable.
    */
    ReportWriter(const std::string& nullString = "#NA") : nullString_(nullString) {}
    virtual ~ReportWriter(){};

    const std::string& nullString() const { return nullString_; }
      
    virtual void writeScenarioReport(ore::data::Report& report,
                                     const std::vector<QuantLib::ext::shared_ptr<SensitivityCube>>& sensitivityCubes,
                                     QuantLib::Real outputThreshold = 0.0);

    virtual void writeAdditionalResultsReport(ore::data::Report& report,
                                              QuantLib::ext::shared_ptr<ore::data::Portfolio> portfolio,
                                              QuantLib::ext::shared_ptr<ore::data::Market> market,
                                              const std::string& configuration, const std::string& baseCurrency,
                                              const std::size_t precision = 6);

    virtual void writeAdditionalResultsPathLevelReport(ore::data::Report& report,
                                                       const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio,
                                                       const std::size_t precision = 6);
    virtual void writeMarketData(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader, const QuantLib::Date& asof,
        const set<string>& quoteNames, bool returnAll);

    virtual void writeFixings(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader);

    virtual void writeDividends(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Loader>& loader);

    virtual void writePricingStats(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);

    virtual void writeRunTimes(ore::data::Report& report, const ore::data::Timer& timer);

    virtual void writeCube(ore::data::Report& report, const QuantLib::ext::shared_ptr<NPVCube>& cube,
                           const std::map<std::string, std::string>& nettingSetMap = std::map<std::string, std::string>());

    virtual void writeStockSplitReport(const QuantLib::ext::shared_ptr<Scenario>& baseScenario,
                                       const QuantLib::ext::shared_ptr<HistoricalScenarioLoader>& hsloader,
                                       const QuantLib::ext::shared_ptr<ore::data::AdjustmentFactors>& adjFactors,
                                       const QuantLib::ext::shared_ptr<ore::data::Report>& report);

    virtual void writeIMScheduleSummaryReport(
        const std::map<SimmConfiguration::SimmSide,
                       std::map<NettingSetDetails, std::pair<CrifRecord::Regulation, IMScheduleResults>>>& finalResultsMap,
        const QuantLib::ext::shared_ptr<ore::data::Report> report, const bool hasNettingSetDetails = false,
        const std::string& simmResultCcy = "", const std::string& reportCcy = "", QuantLib::Real fxSpot = 1.0,
        QuantLib::Real outputThreshold = 0.005);

    virtual void writeIMScheduleTradeReport(const std::map<std::string, std::vector<IMScheduleCalculator::IMScheduleTradeData>>& tradeResults,
                                            const QuantLib::ext::shared_ptr<ore::data::Report> report,
                                            const bool hasNettingSetDetails = false);
                                            
    virtual void
    writePnlReport(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t0NpvReport,
        const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t0m0p0NpvReport,
        const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t1m0p0NpvReport,
        const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t1m1p0NpvReport,
        const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t1m0p1NpvReport,
        const QuantLib::ext::shared_ptr<ore::data::InMemoryReport>& t1m1p1NpvReport,
	    const std::map<std::string, std::vector<ore::data::TradeCashflowReportData>>& t0TradeCashflows,
	    const QuantLib::Date& startDate, const QuantLib::Date& endDate,
	    const std::string& baseCurrency,
	    const QuantLib::ext::shared_ptr<ore::data::Market>& market, const std::string& configuration,
                   const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);    

    void writeXmlReport(ore::data::Report& report, std::string header, std::string xmlString);

    void writeModelCalibrationReport(ore::data::Report& report, const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);

    void writeModelCalibrationDetailReport(ore::data::Report& report,
                                           const QuantLib::ext::shared_ptr<ore::data::Portfolio>& portfolio);

protected:
    std::string nullString_;
    void addMarketDatum(ore::data::Report& report, const ore::data::MarketDatum& md,
                        const QuantLib::Date& actualDate = QuantLib::Date());
};

} // namespace analytics
} // namespace ore
