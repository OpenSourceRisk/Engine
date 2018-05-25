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
#include <orea/simulation/dategrid.hpp>
#include <ored/marketdata/market.hpp>
#include <ored/marketdata/todaysmarketparameters.hpp>
#include <ored/portfolio/portfolio.hpp>
#include <ored/report/report.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <string>

namespace ore {
namespace analytics {
//! Write ORE outputs to reports
/*! \ingroup app
*/
class ReportWriter {
public:
    virtual ~ReportWriter() {};

    virtual void writeNpv(ore::data::Report& report, const std::string& baseCurrency,
                            boost::shared_ptr<ore::data::Market> market, const std::string& configuration,
                            boost::shared_ptr<Portfolio> portfolio);

    virtual void writeCashflow(ore::data::Report& report, boost::shared_ptr<ore::data::Portfolio> portfolio);

    virtual void writeCurves(ore::data::Report& report, const std::string& configID, const DateGrid& grid,
                            const TodaysMarketParameters& marketConfig, const boost::shared_ptr<Market>& market);

    virtual void writeTradeExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                    const std::string& tradeId);

    virtual void writeNettingSetExposures(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                            const std::string& nettingSetId);

    virtual void writeNettingSetColva(ore::data::Report& report, boost::shared_ptr<PostProcess> postProcess,
                                        const std::string& nettingSetId);

    virtual void writeXVA(ore::data::Report& report, const string& allocationMethod,
                            boost::shared_ptr<Portfolio> portfolio, boost::shared_ptr<PostProcess> postProcess);

    virtual void writeAggregationScenarioData(ore::data::Report& report, const AggregationScenarioData& data);

    virtual void writeScenarioReport(ore::data::Report& report, 
        const boost::shared_ptr<SensitivityCube>& sensitivityCube, QuantLib::Real outputThreshold = 0.0);

    // Note: empty braces for default map argument will not work on Visual Studio
    //       https://stackoverflow.com/a/28837927/1771882
    // TODO: do we really need the actual shift sizes in the sensitivity report? Would it be better to have 
    //       shift size and shift type? Could make these part of Scenario Descriptions - would be cleaner.
    virtual void writeSensitivityReport(ore::data::Report& report,
        const boost::shared_ptr<SensitivityCube>& sensitivityCube, QuantLib::Real outputThreshold = 0.0,
        const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes = std::map<RiskFactorKey, QuantLib::Real>());

    virtual void writeCrossGammaReport(ore::data::Report& report,
        const boost::shared_ptr<SensitivityCube>& sensitivityCube, QuantLib::Real outputThreshold = 0.0,
        const std::map<RiskFactorKey, QuantLib::Real>& shiftSizes = std::map<RiskFactorKey, QuantLib::Real>());

};
} // namespace analytics
} // namespace ore
