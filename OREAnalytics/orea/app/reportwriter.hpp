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
  \ingroup cube
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include <map>
#include <orea/aggregation/postprocess.hpp>
#include <orea/app/parameters.hpp>
#include <orea/cube/npvcube.hpp>
#include <orea/simulation/dategrid.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <string>

namespace ore {
namespace analytics {

class ReportWriter {
public:
    static void writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                         boost::shared_ptr<ore::data::Market> market, const std::string& configuration,
                         boost::shared_ptr<Portfolio> portfolio);

    static void writeCashflow(ore::data::Report& report, boost::shared_ptr<ore::data::Portfolio> portfolio);

    static void writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                            const TodaysMarketParameters& marketConfig, const boost::shared_ptr<Market>& market);

    static void writeTradeExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                    const std::string& tradeId);

    static void writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                         const std::string& nettingSetId);

    static void writeNettingSetColva(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                     const std::string& nettingSetId);

    static void writeXVA(ore::data::Report& report, const string& allocationMethod,
                         boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<PostProcess> postProcess);

    static void writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data);

private:
    //! ctor
    ReportWriter(){};
    ReportWriter(const ReportWriter&);
    ReportWriter& operator=(const ReportWriter&);
};
} // namespace analytics
} // namespace ore
