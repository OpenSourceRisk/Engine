/*
   Copyright (C) 2017 Quaternion Risk Management Ltd
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
    //! ctor
    ReportWriter(){};

    void writeNpv(const Parameters& params, boost::shared_ptr<ore::data::Market> market,
                  const std::string& configuration, boost::shared_ptr<Portfolio> portfolio,
                  boost::shared_ptr<ore::data::Report> report);

    void writeCashflow(boost::shared_ptr<ore::data::Portfolio> portfolio, boost::shared_ptr<ore::data::Report> report);

    void writeCurves(const Parameters& params, const TodaysMarketParameters& marketConfig,
                     const boost::shared_ptr<Market>& market, boost::shared_ptr<ore::data::Report> report);

    void writeTradeExposures(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report,
                             const std::string& tradeId);

    void writeNettingSetExposures(boost::shared_ptr<PostProcess> postProcess,
                                  boost::shared_ptr<ore::data::Report> report, const std::string& nettingSetId);

    void writeNettingSetColva(boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report,
                              const std::string& nettingSetId);

    void writeXVA(const Parameters& params, boost::shared_ptr<Portfolio> portfolio,
                  boost::shared_ptr<PostProcess> postProcess, boost::shared_ptr<ore::data::Report> report);
};
}
}
