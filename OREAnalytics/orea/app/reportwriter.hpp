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

#include <boost/shared_ptr.hpp>
#include <map>
#include <orea/aggregation/postprocess.hpp>
#include <orea/app/parameters.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/cube/sensitivitycube.hpp>
#include <orea/engine/sensitivitystream.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/marketdata/todaysmarketcalibrationinfo.hpp>
#include <ored/marketdata/loader.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/dategrid.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <string>

namespace ore {
namespace analytics {
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

    virtual void writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                          boost::shared_ptr<ore::data::Market> market, const std::string& configuration,
                          boost::shared_ptr<Portfolio> portfolio);

    virtual void writeCashflow(ore::data::Report& report, boost::shared_ptr<ore::data::Portfolio> portfolio,
                               boost::shared_ptr<ore::data::Market> market = boost::shared_ptr<ore::data::Market>(),
                               const std::string& configuration = ore::data::Market::defaultConfiguration,
                               const bool includePastCashflows = false);

    virtual void writeCashflowNpv(ore::data::Report& report,
                                  const ore::data::InMemoryReport& cashflowReport,
                                  boost::shared_ptr<ore::data::Market> market,
                                  const std::string& configuration,
                                  const std::string& baseCcy,
                                  const Date& horizon = Date::maxDate());

    virtual void writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                             const TodaysMarketParameters& marketConfig, const boost::shared_ptr<Market>& market,
                             const bool continueOnError = false);

    virtual void writeTradeExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                     const std::string& tradeId);

    virtual void writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                          const std::string& nettingSetId);

    virtual void writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess);
    
    virtual void writeNettingSetCvaSensitivities(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                          const std::string& nettingSetId);

    virtual void writeNettingSetColva(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                      const std::string& nettingSetId);

    virtual void writeXVA(ore::data::Report& report, const string& allocationMethod,
                          boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<PostProcess> postProcess);

    virtual void writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data);

    virtual void writeScenarioReport(ore::data::Report& report,
                                     const boost::shared_ptr<SensitivityCube>& sensitivityCube,
                                     QuantLib::Real outputThreshold = 0.0);

    virtual void writeSensitivityReport(ore::data::Report& report, const boost::shared_ptr<SensitivityStream>& ss,
                                        QuantLib::Real outputThreshold = 0.0, QuantLib::Size outputPrecision = 2);

    virtual void writeAdditionalResultsReport(ore::data::Report& report, boost::shared_ptr<ore::data::Portfolio> portfolio,
                                        boost::shared_ptr<Market> market, const std::string& baseCurrency);

    virtual void
    writeTodaysMarketCalibrationReport(ore::data::Report& report,
                                       boost::shared_ptr<ore::data::TodaysMarketCalibrationInfo> calibrationInfo);

    virtual void writeMarketData(ore::data::Report& report, const boost::shared_ptr<ore::data::Loader>& loader, const QuantLib::Date& asof,
        const set<string>& quoteNames, bool returnAll);

    virtual void writeFixings(ore::data::Report& report, const boost::shared_ptr<ore::data::Loader>& loader);

    virtual void writeDividends(ore::data::Report& report, const boost::shared_ptr<ore::data::Loader>& loader);

    virtual void writePricingStats(ore::data::Report& report, const boost::shared_ptr<Portfolio>& portfolio);

    const std::string& nullString() const { return nullString_; }

protected:
    std::string nullString_;
    void addMarketDatum(ore::data::Report& report, const ore::data::MarketDatum& md);
};

} // namespace analytics
} // namespace ore
